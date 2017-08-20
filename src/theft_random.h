#ifndef THEFT_RANDOM_H
#define THEFT_RANDOM_H

#include "theft.h"
#include "theft_autoshrink.h"

/* Inject a bit pool for autoshrinking -- Get the random bit stream from
 * it, rather than the PRNG, because we'll shrink by shrinking the bit
 * pool itself. */
void theft_random_inject_autoshrink_bit_pool(struct theft *t,
    struct autoshrink_bit_pool *bitpool);

/* Stop using an autoshrink bit pool.
 * (Re-seeding the PRNG will also do this.) */
void theft_random_stop_using_bit_pool(struct theft *t);

/* (Re-)initialize the random number generator with a specific seed.
 * This stops using the current bit pool. */
void theft_random_set_seed(struct theft *t, uint64_t seed);

#endif
