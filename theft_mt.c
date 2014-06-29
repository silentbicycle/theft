/* 
   A C-program for MT19937, with initialization improved 2002/1/26.
   Coded by Takuji Nishimura and Makoto Matsumoto.

   Before using, initialize the state by using init_genrand(seed)  
   or init_by_array(init_key, key_length).

   Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
   All rights reserved.                          

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     1. Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

     2. Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

     3. The names of its contributors may not be used to endorse or promote 
        products derived from this software without specific prior written 
        permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


   Any feedback is very welcome.
   http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html
   email: m-mat @ math.sci.hiroshima-u.ac.jp (remove space)
*/

/* The code has been modified to store internal state in heap/stack
 * allocated memory, rather than statically allocated memory, to allow
 * multiple instances running in the same address space. */

#include <stdlib.h>
#include "theft_mt.h"

#define N THEFT_MT_PARAM_N
#define M 397
#define MATRIX_A 0x9908b0dfUL   /* constant vector a */
#define UPPER_MASK 0x80000000UL /* most significant w-r bits */
#define LOWER_MASK 0x7fffffffUL /* least significant r bits */

/* Heap-allocate a mersenne twister struct. */
struct theft_mt *theft_mt_init(uint32_t seed) {
    struct theft_mt *mt = malloc(sizeof(struct theft_mt));
    if (mt == NULL) { return NULL; }
    theft_mt_reset(mt, seed);
    return mt;
}

/* Free a heap-allocated mersenne twister struct. */
void theft_mt_free(struct theft_mt *mt) {
    free(mt);
}

/* Reset a mersenne twister struct, possibly stack-allocated. */
void theft_mt_reset(struct theft_mt *mt, uint32_t seed) {
    mt->mt[0]= seed & 0xffffffffUL;
    for (mt->mti=1; mt->mti<N; mt->mti++) {
        mt->mt[mt->mti] = 
	    (1812433253UL * (mt->mt[mt->mti-1] ^ (mt->mt[mt->mti-1] >> 30))
                + mt->mti);
        /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
        /* In the previous versions, MSBs of the seed affect   */
        /* only MSBs of the array mt[].                        */
        /* 2002/01/09 modified by Makoto Matsumoto             */
        mt->mt[mt->mti] &= 0xffffffffUL;
        /* for >32 bit machines */
    }
}

static uint32_t mt_genrand_int32(struct theft_mt *mt)
{
    uint32_t y;
    static uint32_t mag01[2]={0x0UL, MATRIX_A};
    /* mag01[x] = x * MATRIX_A  for x=0,1 */

    if (mt->mti >= N) { /* generate N words at one time */
        int kk;

        for (kk=0;kk<N-M;kk++) {
            y = (mt->mt[kk]&UPPER_MASK)|(mt->mt[kk+1]&LOWER_MASK);
            mt->mt[kk] = mt->mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        for (;kk<N-1;kk++) {
            y = (mt->mt[kk]&UPPER_MASK)|(mt->mt[kk+1]&LOWER_MASK);
            mt->mt[kk] = mt->mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        y = (mt->mt[N-1]&UPPER_MASK)|(mt->mt[0]&LOWER_MASK);
        mt->mt[N-1] = mt->mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1UL];

        mt->mti = 0;
    }
  
    y = mt->mt[mt->mti++];

    /* Tempering */
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680UL;
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y >> 18);

    return y;
}

/* Get a 64-bit random number. */
uint64_t theft_mt_random(struct theft_mt *mt) {
    /* Ensure deterministic call order for lower and upper. */
    uint64_t res = mt_genrand_int32(mt);
    res |= (uint64_t)(mt_genrand_int32(mt)) << 32;
    return res;
}
