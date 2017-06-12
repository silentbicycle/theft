#include "theft_random.h"

#include "theft_types_internal.h"
#include "theft_mt.h"

#include <inttypes.h>
#include <assert.h>

static uint64_t get_mask(uint8_t bits);

/* (Re-)initialize the random number generator with a specific seed.
 * This stops using the current bit pool. */
void theft_random_set_seed(struct theft *t, uint64_t seed) {
    theft_random_stop_using_bit_pool(t);
    t->seed = seed;
    t->prng_buf = 0;
    t->bits_available = 0;

    theft_mt_reset(t->mt, seed);
    LOG(2, "%s: SET_SEED: %" PRIx64 "\n", __func__, seed);
}

void theft_random_inject_autoshrink_bit_pool(struct theft *t,
        struct theft_autoshrink_bit_pool *bit_pool) {
    t->bit_pool = bit_pool;
}

void theft_random_stop_using_bit_pool(struct theft *t) {
    t->bit_pool = NULL;
}

/* Get BITS random bits from the test runner's PRNG.
 * Bits can be retrieved at most 64 at a time. */
uint64_t theft_random_bits(struct theft *t, uint8_t bit_count) {
    if (bit_count > 64) {
        LOG(0, "BIT COUNT: %u\n", bit_count);
        assert(bit_count <= 64);
    }
    LOG(4, "RANDOM_BITS: available %u, bit_count: %u, buf %016" PRIx64 "\n",
        t->bits_available, bit_count, t->prng_buf);

    uint64_t res = 0;
    theft_random_bits_bulk(t, bit_count, &res);
    return res;

}

void theft_random_bits_bulk(struct theft *t, uint32_t bit_count, uint64_t *buf) {
    assert(buf);
    if (t->bit_pool) {
        theft_autoshrink_bit_pool_random(t, t->bit_pool, bit_count, true, buf);
        return;
    }

    size_t rem = bit_count;
    uint8_t shift = 0;
    size_t offset = 0;

    while (rem > 0) {
        if (t->bits_available == 0) {
            t->prng_buf = theft_mt_random(t->mt);
            t->bits_available = 64;
        }

        uint8_t take = 64 - shift;
        if (take > rem) {
            take = rem;
        }
        if (take > t->bits_available) {
            take = t->bits_available;
        }

        LOG(5, "%s: rem %zd, available %u, buf 0x%016" PRIx64 ", offset %zd, take %u\n",
            __func__, rem, t->bits_available, t->prng_buf, offset, take);

        const uint64_t mask = get_mask(take);
        buf[offset] |= (t->prng_buf & mask) << shift;
        t->bits_available -= take;
        t->prng_buf >>= take;

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

/* Get a random double from the test runner's PRNG. */
double theft_random_double(struct theft *t) {
    double res = theft_mt_uint64_to_double(theft_random_bits(t, 64));
    LOG(4, "RANDOM_DOUBLE: %g\n", res);
    return res;
}

static uint64_t get_mask(uint8_t bits) {
    if (bits == 64) {
        return ~(uint64_t)0;    // just set all bits -- would overflow
    } else {
        return (1LLU << bits) - 1;
    }
}
