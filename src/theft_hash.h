#ifndef THEFT_HASH_H
#define THEFT_HASH_H

#include "theft_types_internal.h"

/* A hash of an instance. */
typedef uint64_t theft_hash;


/* Internal state for incremental hashing. */
struct theft_hasher {
    theft_hash accum;
};

/* Hash a buffer in one pass. (Wraps the below functions.) */
theft_hash theft_hash_onepass(const uint8_t *data, size_t bytes);

/* Initialize/reset a hasher for incremental hashing. */
void theft_hash_init(struct theft_hasher *h);

/* Sink more data into an incremental hash. */
void theft_hash_sink(struct theft_hasher *h,
    const uint8_t *data, size_t bytes);

/* Finish hashing and get the result.
 * (This also resets the internal hasher state.) */
theft_hash theft_hash_done(struct theft_hasher *h);


#endif
