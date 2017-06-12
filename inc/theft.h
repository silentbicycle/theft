#ifndef THEFT_H
#define THEFT_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* Version 0.2.0 */
#define THEFT_VERSION_MAJOR 0
#define THEFT_VERSION_MINOR 2
#define THEFT_VERSION_PATCH 0

/* A property can have at most this many arguments. */
#define THEFT_MAX_ARITY 10

#include "theft_types.h"

/* Get a random 64-bit integer from the test runner's PRNG.
 *
 * NOTE: This is equivalent to `theft_random_bits(t, 64)`, and
 * will be removed in a future release. */
uint64_t theft_random(struct theft *t);

/* Get BITS random bits from the test runner's PRNG.
 * Bits can be retrieved at most 64 at a time. */
uint64_t theft_random_bits(struct theft *t, uint8_t bits);

/* Get BITS random bits, in bulk, and put them in BUF.
 * BUF is assumed to be large enough. */
void theft_random_bits_bulk(struct theft *t, uint32_t bits, uint64_t *buf);

/* Get a random double from the test runner's PRNG. */
double theft_random_double(struct theft *t);

/* Change T's output stream handle to OUT. (Default: stdout.) */
void theft_set_output_stream(struct theft *t, FILE *out);

/* Run a series of randomized trials of a property function.
 *
 * Configuration is specified in CFG; many fields are optional.
 * See the type definition in `theft_types.h`. */
enum theft_run_res
theft_run(const struct theft_run_config *cfg);

/* Hash a buffer in one pass. (Wraps the below functions.) */
theft_hash theft_hash_onepass(const uint8_t *data, size_t bytes);

/* Initialize/reset a hasher for incremental hashing. */
void theft_hash_init(struct theft_hasher *h);

/* Sink more data into an incremental hash. */
void theft_hash_sink(struct theft_hasher *h,
    const uint8_t *data, size_t bytes);

/* Finish hashing and get the result. */
theft_hash theft_hash_done(struct theft_hasher *h);

#endif
