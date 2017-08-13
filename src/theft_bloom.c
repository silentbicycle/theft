#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "theft.h"
#include "theft_bloom.h"
#include "theft_types_internal.h"

/* This is a dynamic blocked bloom filter, loosely based on
 * _Cache Efficient Bloom Filters for Shared Memory Machines_
 * by Tim Kaler.
 *
 * The top level of the bloom filter uses the first N bits of the hash
 * (top_block2) to choose between (1 << N) distinct bloom filter blocks.
 * These blocks are created as necessary, i.e., a NULL block means no
 * bits in that block would have been set.
 *
 * When checking for matches, HASH_COUNT different chunks of M bits from
 * the hash are used to check each block's bloom filter. (M is
 * block->size2, and the bloom filter has (1 << M) bits.) If any of
 * the selected bits are false, there was no match. Every bloom filter
 * in the block's linked list is checked, so all must match for
 * `theft_bloom_check` to return true.
 *
 * When marking, only the front (largest) bloom filter in the
 * appropriate block is updated. If marking did not change any
 * bits (all bits chosen by the hash were already set), then
 * the bloom filter is considered too full, and a new one is
 * inserted before it, as the new head of the block. The new
 * bloom filter's size2 is one larger, so more bits of the hash
 * are used, and the bloom filter doubles in size. */

/* Default number of bits to use for choosing a specific
 * block (linked list of bloom filters) */
#define DEF_TOP_BLOCK_BITS 9

/* Default number of bits in each first-layer bloom filter */
#define DEF_MIN_FILTER_BITS 9

/* How many hashes to check for each block */
#define HASH_COUNT 4

#define LOG_BLOOM 0

struct bloom_filter {
    struct bloom_filter *next;
    uint8_t size2;              /* log2 of bit count */
    uint8_t bits[];
};

struct theft_bloom {
    const uint8_t top_block2;
    const uint8_t min_filter2;
    /* These start as NULL and are lazily allocated.
     * Each block is a linked list of bloom filters, with successively
     * larger filters appended at the front as the filters fill up. */
    struct bloom_filter *blocks[];
};

static struct theft_bloom_config def_config = { .top_block_bits = 0 };

/* Initialize a dynamic blocked bloom filter. */
struct theft_bloom *theft_bloom_init(const struct theft_bloom_config *config) {
#define DEF(X, DEFAULT) (X ? X : DEFAULT)
    config = DEF(config, &def_config);
    const uint8_t top_block2 = DEF(config->top_block_bits, DEF_TOP_BLOCK_BITS);
    const uint8_t min_filter2 = DEF(config->min_filter_bits, DEF_MIN_FILTER_BITS);
#undef DEF

    const size_t top_block_count = (1LLU << top_block2);
    const size_t alloc_size = sizeof(struct theft_bloom) +
      top_block_count * sizeof(struct bloom_filter *);

    struct theft_bloom *res = malloc(alloc_size);
    if (res == NULL) {
        return NULL;
    }
    memset(&res->blocks, 0x00, top_block_count * sizeof(struct bloom_filter *));

    struct theft_bloom b = {
        .top_block2 = top_block2,
        .min_filter2 = min_filter2,
    };
    memcpy(res, &b, sizeof(b));
    return res;
}

static struct bloom_filter *
alloc_filter(uint8_t bits) {
    const size_t alloc_size = sizeof(struct bloom_filter)
      + ((1LLU << bits) / 8);
    struct bloom_filter *bf = malloc(alloc_size);
    if (bf != NULL) {
        memset(bf, 0x00, alloc_size);
        bf->size2 = bits;
        LOG(4 - LOG_BLOOM, "%s: %p [size2 %u (%zd bytes)]\n",
            __func__, (void *)bf, bf->size2, (size_t)((1LLU << bf->size2) / 8));
    }
    return bf;
}

/* Hash data and mark it in the bloom filter. */
bool theft_bloom_mark(struct theft_bloom *b, uint8_t *data, size_t data_size) {
    uint64_t hash = theft_hash_onepass(data, data_size);
    const size_t top_block_count = (1LLU << b->top_block2);
    LOG(3 - LOG_BLOOM,
        "%s: overall hash: 0x%016" PRIx64 "\n", __func__, hash);

    const uint64_t top_block_mask = top_block_count - 1;
    const size_t block_id = hash & top_block_mask;
    LOG(3 - LOG_BLOOM, "%s: block_id %zd\n", __func__, block_id);

    struct bloom_filter *bf = b->blocks[block_id];
    if (bf == NULL) {           /* lazily allocate */
        bf = alloc_filter(b->min_filter2);
        if (bf == NULL) {
            return false;       /* alloc fail */
        }
        b->blocks[block_id] = bf;
    }

    /* Must be able to do all checks with one 64 bit hash.
     * In order to relax this restriction, theft's hashing
     * code will need to be restructured to give the bloom
     * filter code two independent hashes. */
    assert(64 - b->top_block2 - (HASH_COUNT * bf->size2) > 0);
    hash >>= b->top_block2;

    const uint8_t block_size2 = bf->size2;
    const uint64_t block_mask = (1LLU << block_size2) - 1;
    bool any_set = false;

    /* Only mark in the front filter. */
    for (size_t i = 0; i < HASH_COUNT; i++) {
        const uint64_t v = (hash >> (i*block_size2)) & block_mask;
        const uint64_t offset = v / 8;
        const uint8_t bit = 1 << (v & 0x07);
        LOG(4 - LOG_BLOOM,
            "%s: marking %p @ %" PRIu64 " =>  offset %" PRIu64
            ", bit 0x%02x\n",
            __func__, (void *)bf, v, offset, bit);
        if (0 == (bf->bits[offset] & bit)) {
            any_set = true;
        }
        bf->bits[offset] |= bit;
    }

    /* If all bits were already set, prepend a new, empty filter -- the
     * previous filter will still match when checking, but there will be
     * a reduced chance of false positives for new entries. */
    if (!any_set) {
        if (b->top_block2 + HASH_COUNT * (bf->size2 + 1) > 64) {
            /* We can't grow this hash chain any further with the
             * hash bits available. */
            LOG(0, "%s: Warning: bloom filter block %zd cannot grow further!\n",
                __func__, block_id);
        } else {
            struct bloom_filter *nbf = alloc_filter(bf->size2 + 1);
            LOG(3 - LOG_BLOOM, "%s: growing bloom filter -- bits %u, nbf %p\n",
                __func__, bf->size2 + 1, (void *)nbf);
            if (nbf == NULL) {
                return false;       /* alloc fail */
            }
            nbf->next = bf;
            b->blocks[block_id] = nbf; /* append to front */
        }
    }

    return true;
}

/* Check whether the data's hash is in the bloom filter. */
bool theft_bloom_check(struct theft_bloom *b, uint8_t *data, size_t data_size) {
    uint64_t hash = theft_hash_onepass(data, data_size);
    LOG(3 - LOG_BLOOM,
        "%s: overall hash: 0x%016" PRIx64 "\n", __func__, hash);
    const size_t top_block_count = (1LLU << b->top_block2);
    const uint64_t top_block_mask = top_block_count - 1;
    const size_t block_id = hash & top_block_mask;
    LOG(3 - LOG_BLOOM, "%s: block_id %zd\n", __func__, block_id);

    struct bloom_filter *bf = b->blocks[block_id];
    if (bf == NULL) {
        return false;           /* block not allocated: no bits set */
    }

    hash >>= b->top_block2;

    /* Check every block */
    while (bf != NULL) {
        const uint8_t block_size2 = bf->size2;
        const uint64_t block_mask = (1LLU << block_size2) - 1;

        bool hit_all_in_block = true;
        for (size_t i = 0; i < HASH_COUNT; i++) {
            const uint64_t v = (hash >> (i * block_size2)) & block_mask;
            const uint64_t offset = v / 8;
            const uint8_t bit = 1 << (v & 0x07);
            LOG(4 - LOG_BLOOM,
                "%s: checking %p (bits %u) @ %" PRIu64 " => offset %" PRIu64
                ", bit 0x%02x: 0x%02x\n",
                __func__, (void *)bf, block_size2, v, offset, bit,
                (bf->bits[offset] & bit));
            if (0 == (bf->bits[offset] & bit)) {
                hit_all_in_block = false;
                break;
            }
        }
        if (hit_all_in_block) {
            return true;
        }
        bf = bf->next;
    }

    return false; /* there wasn't any block with all checked bits set */
}

/* Free the bloom filter. */
void theft_bloom_free(struct theft_bloom *b) {
    const size_t top_block_count = (1LLU << b->top_block2);
    uint8_t max_length = 0;
    for (size_t i = 0; i < top_block_count; i++) {
        uint8_t length = 0;
        struct bloom_filter *bf = b->blocks[i];
        while (bf != NULL) {
            struct bloom_filter *next = bf->next;
            free(bf);
            bf = next;
            length++;
        }
        LOG(3 - LOG_BLOOM,
            "%s: block %zd, length %u\n", __func__, i, length);
        max_length = (length > max_length ? length : max_length);
    }
    LOG(3 - LOG_BLOOM,
        "%s: %zd blocks, max length %u\n", __func__, top_block_count, max_length);
    free(b);
}
