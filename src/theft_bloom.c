#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "theft.h"
#include "theft_bloom.h"
#include "theft_types_internal.h"

#define DEFAULT_BLOOM_BITS 17
#define DEBUG_BLOOM_FILTER 0
#define LOG2_BITS_PER_BYTE 3
#define HASH_COUNT 4

/* Initialize a bloom filter. */
struct theft_bloom *theft_bloom_init(uint8_t bit_size2) {
    size_t sz = 1 << (bit_size2 - LOG2_BITS_PER_BYTE);
    struct theft_bloom *b = malloc(sizeof(struct theft_bloom));
    if (b == NULL) {
        return NULL;
    }

    /* Ensure b->bits is 64-bit aligned so we can process it 64 bits at a time */
    uint64_t *bits64 = calloc(sz / 8, sizeof(uint64_t));
    if (bits64 == NULL) {
        free(b);
        return NULL;
    }
    *b = (struct theft_bloom) {
        .bits = (uint8_t *)bits64,
        .size = sz,
        .bit_count = bit_size2,
    };
    return b;
}

/* Hash data and mark it in the bloom filter. */
void theft_bloom_mark(struct theft_bloom *b, uint8_t *data, size_t data_size) {
    uint64_t hash = theft_hash_onepass(data, data_size);
    uint8_t bc = b->bit_count;
    uint64_t mask = (1 << bc) - 1;

    /* Use HASH_COUNT distinct slices of MASK bits as hashes for the bloom filter. */
    int bit_inc = (64 - bc) / HASH_COUNT;

    for (int i = 0; i < (64 - bc); i += bit_inc) {
        uint64_t v = (hash & (mask << i)) >> i;
        size_t offset = v / 8;
        uint8_t bit = 1 << (v & 0x07);
        b->bits[offset] |= bit;
    }
}

/* Check whether the data's hash is in the bloom filter. */
bool theft_bloom_check(struct theft_bloom *b, uint8_t *data, size_t data_size) {
    uint64_t hash = theft_hash_onepass(data, data_size);
    LOG(4, "%s: overall hash: 0x%016" PRIx64 "\n", __func__, hash);
    uint8_t bc = b->bit_count;
    uint64_t mask = (1 << bc) - 1;

    int bit_inc = (64 - bc) / HASH_COUNT;

    for (int i = 0; i < (64 - bc); i += bit_inc) {
        uint64_t v = (hash & (mask << i)) >> i;
        size_t offset = v / 8;
        uint8_t bit = 1 << (v & 0x07);
        if (0 == (b->bits[offset] & bit)) { return false; }
    }

    return true;
}

/* Free the bloom filter. */
void theft_bloom_free(struct theft_bloom *b) {
    free(b->bits);
    free(b);
}

#include "bits_lut.h"

/* Dump the bloom filter's contents. (Debugging.) */
void theft_bloom_dump(struct theft_bloom *b) {
    size_t total = 0;
    size_t last_row_total = 0;

    const size_t size = b->size;
    const uint8_t *bits = b->bits;
    const uint64_t *bits64 = (uint64_t *)bits;

    size_t offset = 0;
    const size_t limit = size / 8;
    assert((size % 8) == 0);
    while (offset < limit) {
        uint64_t cur = bits64[offset];
        if ((offset > 0 && (offset & 0x07) == 0) || (offset == limit - 1)) {
            LOG(2 - DEBUG_BLOOM_FILTER,
                " - %2.1f%%\n", (100 * (total - last_row_total)) / (64.0 * 8));
            last_row_total = total;
        }
        if (cur == 0) {
            offset++;
            LOG(2 - DEBUG_BLOOM_FILTER, "........");
        } else {
            for (uint8_t i = 0; i < 8; i++) {
                const uint8_t byte = 0xff & (cur >> (8U*i));
                const uint8_t count = bits_lut[byte];
                LOG(2 - DEBUG_BLOOM_FILTER, "%c", (count == 0 ? '.' : '0' + count));
                total += count;
            }
            offset++;
        }
    }

    LOG(1 - DEBUG_BLOOM_FILTER,
        " -- bloom filter: %zd of %zd bits set (%2d%%)\n",
        total, 8*size, (int)((100.0 * total)/(8.0*size)));

    /* If the total number of bits set is > the number of bytes
     * in the table (i.e., > 1/8 full) then warn the user. */
    if (total > size) {
        fprintf(stderr, "\nWARNING: bloom filter is %zd%% full, "
            "larger bloom_bits value recommended.\n",
            (size_t)((100 * total) / (8 * size)));
    }
    (void)last_row_total;
}

/* Recommend a bloom filter size for a given number of trials. */
uint8_t theft_bloom_recommendation(int trials) {
    /* With a preferred priority of false positives under 0.1%,
     * the required number of bits m in the bloom filter is:
     *     m = -lg(0.001)/(lg(2)^2) == 14.378 ~= 14,
     * so we want an array with 1 << ceil(log2(14*trials)) cells.
     *
     * Note: The above formula is for the *ideal* number of hash
     * functions, but we're using a hardcoded count. It appears to work
     * well enough in practice, though, and this can be adjusted later
     * without impacting the API. (This errs on the side of being too
     * large.) */
    #define TRIAL_MULTIPILER 14
    uint8_t res = DEFAULT_BLOOM_BITS;

    /* Double the number of trials, to account for some shrinking. */
    trials *= 2;

    const uint8_t min = THEFT_BLOOM_BITS_MIN - LOG2_BITS_PER_BYTE;
    const uint8_t max = THEFT_BLOOM_BITS_MAX - LOG2_BITS_PER_BYTE;

    for (uint8_t i = min; i < max; i++) {
        int32_t v = (1 << i);
        if (v > (TRIAL_MULTIPILER * trials)) {
            res = i + LOG2_BITS_PER_BYTE;
            break;
        }
    }

    #if DEBUG_BLOOM_FILTER
    size_t sz = 1 << (res - LOG2_BITS_PER_BYTE);
    printf("Using %zd bytes for bloom filter: %d trials -> bit_size2 %u\n",
        sizeof(struct theft_bloom) + sz, trials, res);
    #endif

    return res;
}
