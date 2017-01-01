#include <assert.h>

#include "theft.h"
#include "theft_types_internal.h"
#include "theft_mt.h"

/* (Re-)initialize the random number generator with a specific seed. */
void theft_set_seed(struct theft *t, uint64_t seed) {
    t->seed = seed;
    t->prng_buf = 0;
    t->bits_available = 0;
    theft_mt_reset(t->mt, seed);
}

static uint64_t mask(uint8_t bits) { return (1L << bits) - 1; }

/* Get BITS random bits from the test runner's PRNG.
 * Bits can be retrieved at most 64 at a time. */
uint64_t theft_random_bits(struct theft *t, uint8_t bit_count) {
    assert(bit_count <= 64);

    uint64_t res = 0;
    uint8_t shift = 0;

    if (t->bits_available < bit_count) {
        res |= t->prng_buf & mask(t->bits_available);
        shift += t->bits_available;
        bit_count -= t->bits_available;
        t->prng_buf = theft_random(t);
        t->bits_available = 64;
    }

    res |= ((t->prng_buf & mask(bit_count)) << shift);
    t->bits_available -= bit_count;
    t->prng_buf >>= bit_count;

    return res;
}

/* Get a random 64-bit integer from the test runner's PRNG. */
theft_seed theft_random(struct theft *t) {
    theft_seed ns = (theft_seed)theft_mt_random(t->mt);
    return ns;
}

/* Get a random double from the test runner's PRNG. */
double theft_random_double(struct theft *t) {
    return theft_mt_random_double(t->mt);
}
