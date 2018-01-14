#ifndef THEFT_RNG_H
#define THEFT_RNG_H

#include <stdint.h>

/* Wrapper for xoroshiro128+. */

/* Opaque type for the state of a PRNG. */
struct theft_rng;

/* Heap-allocate a PRNG state struct. */
struct theft_rng *theft_rng_init(uint64_t seed);

/* Free a heap-allocated PRNG state struct. */
void theft_rng_free(struct theft_rng *rng);

/* Re-seed an existing PRNG state struct. */
void theft_rng_reset(struct theft_rng *mt, uint64_t seed);

/* Get a 64-bit random number. */
uint64_t theft_rng_random(struct theft_rng *mt);

/* Convert a uint64_t to a number in the real interval [0,1]. */
double theft_rng_uint64_to_double(uint64_t x);

#endif
