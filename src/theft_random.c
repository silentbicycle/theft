#include "theft_random.h"

#include "theft_types_internal.h"
#include "theft_mt.h"

#include <inttypes.h>
#include <assert.h>

#if 0
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

static uint64_t mask(uint8_t bits);

/* (Re-)initialize the random number generator with a specific seed.
 * This stops using the current bit pool. */
void theft_random_set_seed(struct theft *t, uint64_t seed) {
    theft_random_stop_using_bit_pool(t);
    t->seed = seed;
    t->prng_buf = seed;
    t->bits_available = 64;
    theft_mt_reset(t->mt, seed);
    LOG("SET_SEED: %" PRIx64 "\n", seed);
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
    assert(bit_count <= 64);
    LOG("RANDOM_BITS: available %u, bit_count: %u, buf %016" PRIx64 "\n",
        t->bits_available, bit_count, t->prng_buf);

    if (t->bit_pool) {
        return theft_autoshrink_bit_pool_random(t->bit_pool, bit_count, true);
    }

    uint64_t res = 0;
    uint8_t shift = 0;

    if (t->bits_available < bit_count) {
        LOG(" -- mask of %" PRIx64 "\n", mask(t->bits_available));
        res |= t->prng_buf & mask(t->bits_available);
        shift += t->bits_available;
        bit_count -= t->bits_available;
        t->prng_buf = theft_mt_random(t->mt);
        t->bits_available = 64;
        LOG(" -- NEW BUF %016" PRIx64 ", partial res %016" PRIx64 "\n",
            t->prng_buf, res);
    }

    res |= ((t->prng_buf & mask(bit_count)) << shift);
    t->bits_available -= bit_count;
    t->prng_buf >>= bit_count;
    LOG(" -- res %016" PRIx64 ", bit_count %u, shift %u, buf %016" PRIx64 "\n",
        res, bit_count, shift);
    LOG(" -- shifted buf: %" PRIx64 "\n", t->prng_buf);

    LOG(" -- RANDOM_BITS %d -> result %016" PRIx64 "\n", bit_count, res);
    return res;
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
    LOG("RANDOM_DOUBLE: %g\n", res);
    return res;
}

static uint64_t mask(uint8_t bits) {
    if (bits == 64) {
        return ~(uint64_t)0;    // just set all bits -- would overflow
    } else {
        return (1LLU << bits) - 1;
    }
}
