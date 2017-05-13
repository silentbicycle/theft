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

/* Default number of trials to run. */
#define THEFT_DEF_TRIALS 100

/* Min and max bits used to determine bloom filter size.
 * (A larger value uses more memory, but reduces the odds of an
 * untested argument combination being falsely skipped.) */
#define THEFT_BLOOM_BITS_MIN 13 /* 1 KB */
#define THEFT_BLOOM_BITS_MAX 33 /* 1 GB */

/* Initialize a theft test runner, with the configuration
 * in CFG. If CFG is NULL, a default will be used.
 *
 * Returns a NULL if malloc fails or the provided configuration
 * is invalid. */
struct theft *theft_init(const struct theft_config *cfg);

/* Free a property-test runner. */
void theft_free(struct theft *t);

/* Get a random 64-bit integer from the test runner's PRNG.
 *
 * NOTE: This is equivalent to `theft_random_bits(t, 64)`, and
 * will be removed in a future release. */
uint64_t theft_random(struct theft *t);

/* Get BITS random bits from the test runner's PRNG.
 * Bits can be retrieved at most 64 at a time. */
uint64_t theft_random_bits(struct theft *t, uint8_t bits);

/* Get a random double from the test runner's PRNG. */
double theft_random_double(struct theft *t);

/* Change T's output stream handle to OUT. (Default: stdout.) */
void theft_set_output_stream(struct theft *t, FILE *out);

/* Run a series of randomized trials of a property function.
 *
 * Configuration is specified in CFG; many fields are optional.
 * See the type definition in `theft_types.h`. */
enum theft_run_res
theft_run(struct theft *t, const struct theft_run_config *cfg);

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
