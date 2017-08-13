#ifndef THEFT_BLOOM_H
#define THEFT_BLOOM_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/* Opaque type for bloom filter. */
struct theft_bloom;

struct theft_bloom_config {
    uint8_t top_block_bits;
    uint8_t min_filter_bits;
};

/* Initialize a bloom filter. */
struct theft_bloom *theft_bloom_init(const struct theft_bloom_config *config);

/* Hash data and mark it in the bloom filter. */
bool theft_bloom_mark(struct theft_bloom *b, uint8_t *data, size_t data_size);

/* Check whether the data's hash is in the bloom filter. */
bool theft_bloom_check(struct theft_bloom *b, uint8_t *data, size_t data_size);

/* Free the bloom filter. */
void theft_bloom_free(struct theft_bloom *b);

#endif
