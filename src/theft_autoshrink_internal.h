#ifndef THEFT_AUTOSHRINK_INTERNAL_H
#define THEFT_AUTOSHRINK_INTERNAL_H

#include "theft_types_internal.h"
#include "theft_autoshrink.h"

#include <assert.h>

static struct autoshrink_bit_pool *
alloc_bit_pool(size_t size, size_t limit,
    size_t request_ceil);

static enum theft_alloc_res
alloc_from_bit_pool(struct theft *t, struct autoshrink_env *env,
    struct autoshrink_bit_pool *bit_pool, void **output,
    bool shrinking);

static bool append_request(struct autoshrink_bit_pool *pool,
    uint32_t bit_count);

static void drop_from_bit_pool(struct theft *t,
    struct autoshrink_env *env,
    const struct autoshrink_bit_pool *orig,
    struct autoshrink_bit_pool *pool);

static void mutate_bit_pool(struct theft *t,
    struct autoshrink_env *env,
    const struct autoshrink_bit_pool *orig,
    struct autoshrink_bit_pool *pool);

static bool choose_and_mutate_request(struct theft *t,
    struct autoshrink_env *env,
    const struct autoshrink_bit_pool *orig,
    struct autoshrink_bit_pool *pool);

static bool build_index(struct autoshrink_bit_pool *pool);

static size_t offset_of_pos(const struct autoshrink_bit_pool *orig,
    size_t pos);

static void convert_bit_offset(size_t bit_offset,
    size_t *byte_offset, uint8_t *bit);

static uint64_t
read_bits_at_offset(const struct autoshrink_bit_pool *pool,
    size_t bit_offset, uint8_t size);

static void
write_bits_at_offset(struct autoshrink_bit_pool *pool,
    size_t bit_offset, uint8_t size, uint64_t bits);

static void truncate_trailing_zero_bytes(struct autoshrink_bit_pool *pool);

static void init_model(struct autoshrink_env *env);

static enum mutation
get_weighted_mutation(struct theft *t, struct autoshrink_env *env);

static bool should_drop(struct theft *t, struct autoshrink_env *env,
    size_t request_count);

static void lazily_fill_bit_pool(struct theft *t,
    struct autoshrink_bit_pool *pool,
    const uint32_t bit_count);

static void fill_buf(struct autoshrink_bit_pool *pool,
    const uint32_t bit_count, uint64_t *buf);

#endif
