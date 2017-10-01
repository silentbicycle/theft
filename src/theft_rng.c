/*
 * Copyright (c) 2017, Koz Ross <koz.ross@retro-freedom.nz>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include "theft_rng.h"

struct theft_rng {
  uint64_t state[2];
};

struct theft_rng *theft_rng_init(uint64_t seed) {
  struct theft_rng *rng = malloc(sizeof(struct theft_rng));
  theft_rng_reset(rng, seed);
  return rng;
}

void theft_rng_free(struct theft_rng *rng) {
  free(rng);
}

void theft_rng_reset(struct theft_rng *rng, uint64_t seed) {
  uint64_t candidate_state[2] = { seed, ~seed };
  /* Ensures that bad seeds don't affect the generator */
  uint64_t scramble[2] = { 0x652d97325be35de4, 0x5cd0b453d2d1d };
  for (int i = 0; i < 2; i++) {
    rng->state[i] = candidate_state[i] ^ scramble[i];
  }
}

uint64_t theft_rng_random(struct theft_rng *rng) {
  uint64_t s0 = rng->state[0];
  uint64_t s1 = rng->state[1];
  uint64_t result = s0 + s1;
  s1 ^= s0;
  rng->state[0] = ((s0 << 55) | (s0 >> 9)) ^ s1 ^ (s1 << 14);
  rng->state[1] = (s1 << 36) | (s1 >> 28);
  return result;
}

double theft_rng_uint64_to_double(uint64_t x) {
  return (x >> 11) * (1.0/9007199254740991.0);
}
