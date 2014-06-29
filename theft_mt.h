#ifndef THEFT_MT_H
#define THEFT_MT_H

#include <stdint.h>

/* Wrapper for Mersenne Twister.
 * See copyright and license in theft_mt.c, more details at:
 *     http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html
 *
 * The code has been modified to store internal state in heap/stack
 * allocated memory, rather than statically allocated memory, to allow
 * multiple instances running in the same address space. */

#define THEFT_MT_PARAM_N 624

struct theft_mt {
    uint32_t mt[THEFT_MT_PARAM_N]; /* the array for the state vector  */
    int32_t mti;
};

/* Heap-allocate a mersenne twister struct. */
struct theft_mt *theft_mt_init(uint32_t seed);

/* Free a heap-allocated mersenne twister struct. */
void theft_mt_free(struct theft_mt *mt);

/* Reset a mersenne twister struct, possibly stack-allocated. */
void theft_mt_reset(struct theft_mt *mt, uint32_t seed);

/* Get a 64-bit random number. */
uint64_t theft_mt_random(struct theft_mt *mt);

#endif
