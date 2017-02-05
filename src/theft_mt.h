#ifndef THEFT_MT_H
#define THEFT_MT_H

#include <stdint.h>

/* Wrapper for Mersenne Twister.
 * See copyright and license in theft_mt.c, more details at:
 *     http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html
 *
 * Local modifications are described in theft_mt.c. */

/* Opaque type for a Mersenne Twister PRNG. */
struct theft_mt;

/* Heap-allocate a mersenne twister struct. */
struct theft_mt *theft_mt_init(uint64_t seed);

/* Free a heap-allocated mersenne twister struct. */
void theft_mt_free(struct theft_mt *mt);

/* Reset a mersenne twister struct, possibly stack-allocated. */
void theft_mt_reset(struct theft_mt *mt, uint64_t seed);

/* Get a 64-bit random number. */
uint64_t theft_mt_random(struct theft_mt *mt);

/* Convert a uint64_t to a number on the [0,1]-real-interval. */
double theft_mt_uint64_to_double(uint64_t x);

#endif
