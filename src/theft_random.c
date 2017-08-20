#include "theft_random.h"

#include "theft_types_internal.h"
#include "theft_rng.h"

#include <inttypes.h>
#include <assert.h>

static uint64_t get_mask(uint8_t bits);

/* (Re-)initialize the random number generator with a specific seed.
 * This stops using the current bit pool. */
void theft_random_set_seed(struct theft *t, uint64_t seed) {
    theft_random_stop_using_bit_pool(t);
    t->prng.buf = 0;
    t->prng.bits_available = 0;

    theft_rng_reset(t->prng.rng, seed);
    LOG(2, "%s: SET_SEED: %" PRIx64 "\n", __func__, seed);
}

void theft_random_inject_autoshrink_bit_pool(struct theft *t,
        struct autoshrink_bit_pool *bit_pool) {
    t->prng.bit_pool = bit_pool;
}

void theft_random_stop_using_bit_pool(struct theft *t) {
    t->prng.bit_pool = NULL;
}

/* Get BITS random bits from the test runner's PRNG.
 * Bits can be retrieved at most 64 at a time. */
uint64_t theft_random_bits(struct theft *t, uint8_t bit_count) {
    assert(bit_count <= 64);
    LOG(4, "RANDOM_BITS: available %u, bit_count: %u, buf %016" PRIx64 "\n",
        t->prng.bits_available, bit_count, t->prng.buf);

    uint64_t res = 0;
    theft_random_bits_bulk(t, bit_count, &res);
    return res;

}

void theft_random_bits_bulk(struct theft *t, uint32_t bit_count, uint64_t *buf) {
    LOG(5, "%s: bit_count %u\n", __func__, bit_count);
    assert(buf);
    if (t->prng.bit_pool) {
        theft_autoshrink_bit_pool_random(t, t->prng.bit_pool, bit_count, true, buf);
        return;
    }

    uint32_t rem = bit_count;
    uint8_t shift = 0;
    size_t offset = 0;

    while (rem > 0) {
        if (t->prng.bits_available == 0) {
            t->prng.buf = theft_rng_random(t->prng.rng);
            t->prng.bits_available = 64;
        }
        LOG(5, "%% buf 0x%016" PRIx64 "\n", t->prng.buf);

        uint8_t take = 64 - shift;
        if (take > rem) {
            take = rem;
        }
        if (take > t->prng.bits_available) {
            take = t->prng.bits_available;
        }

        LOG(5, "%s: rem %u, available %u, buf 0x%016" PRIx64 ", offset %zd, take %u\n",
            __func__, rem, t->prng.bits_available, t->prng.buf, offset, take);

        const uint64_t mask = get_mask(take);
        buf[offset] |= (t->prng.buf & mask) << shift;
        LOG(5, "== buf[%zd]: %016" PRIx64 " (%u / %u)\n",
            offset, buf[offset], bit_count - rem, bit_count);
        t->prng.bits_available -= take;
        t->prng.buf >>= take;

        shift += take;
        if (shift == 64) {
            offset++;
            shift = 0;
        }

        rem -= take;
    }
}

/* Get a random 64-bit integer from the test runner's PRNG.
 *
 * NOTE: This is equivalent to `theft_random_bits(t, 64)`, and
 * will be removed in a future release. */
theft_seed theft_random(struct theft *t) {
    return theft_random_bits(t, 8*sizeof(uint64_t));
}

#if THEFT_USE_FLOATING_POINT
/* Get a random double from the test runner's PRNG. */
double theft_random_double(struct theft *t) {
    double res = theft_rng_uint64_to_double(theft_random_bits(t, 64));
    LOG(4, "RANDOM_DOUBLE: %g\n", res);
    return res;
}

uint64_t theft_random_choice(struct theft *t, uint64_t ceil) {
    if (ceil < 2) { return 0; }
    uint64_t bits;
    double limit;

    /* If ceil is a power of two, just return that many bits. */
    if ((ceil & (ceil - 1)) == 0) {
        uint8_t log2_ceil = 1;
        while (ceil > (1LLU << log2_ceil)) {
            log2_ceil++;
        }
        assert((1LLU << log2_ceil) == ceil);
        return theft_random_bits(t, log2_ceil);
    }

    /* If the choice values are fairly small (which shoud be
     * the common case), sample less than 64 bits to reduce
     * time spent managing the random bitstream. */
    if (ceil < UINT8_MAX) {
        bits = theft_random_bits(t, 16);
        limit = (double)(1LLU << 16);
    } else if (ceil < UINT16_MAX) {
        bits = theft_random_bits(t, 32);
        limit = (double)(1LLU << 32);
    } else {
        bits = theft_random_bits(t, 64);
        limit = (double)UINT64_MAX;
    }

    double mul = (double)bits / limit;
    uint64_t res = (uint64_t)(mul * ceil);
    return res;
}
#endif

static uint64_t get_mask(uint8_t bits) {
    if (bits == 64) {
        return ~(uint64_t)0;    // just set all bits -- would overflow
    } else {
        return (1LLU << bits) - 1;
    }
}
