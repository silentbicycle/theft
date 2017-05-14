#ifndef THEFT_AUTOSHRINK_INTERNAL_H
#define THEFT_AUTOSHRINK_INTERNAL_H

#include "theft_types_internal.h"
#include "theft_autoshrink.h"

#include <assert.h>

static theft_alloc_cb autoshrink_alloc;
static theft_free_cb autoshrink_free;
static theft_hash_cb autoshrink_hash;
static theft_shrink_cb autoshrink_shrink;
static theft_print_cb autoshrink_print;

static struct theft_autoshrink_bit_pool *
init_bit_pool(struct theft *t, size_t size,
    theft_seed seed, size_t request_ceil);
static struct theft_autoshrink_bit_pool *alloc_bit_pool(size_t size,
    size_t request_ceil);

static enum theft_alloc_res
alloc_from_bit_pool(struct theft *t, struct theft_autoshrink_env *env,
    struct theft_autoshrink_bit_pool *bit_pool, void **output);

static bool append_request(struct theft_autoshrink_bit_pool *pool,
    uint32_t bit_count);

static void drop_from_bit_pool(struct theft *t,
    struct theft_autoshrink_env *env,
    const struct theft_autoshrink_bit_pool *orig,
    struct theft_autoshrink_bit_pool *pool);

static void mutate_bit_pool(struct theft *t,
    struct theft_autoshrink_env *env,
    const struct theft_autoshrink_bit_pool *orig,
    struct theft_autoshrink_bit_pool *pool);

static bool choose_and_mutate_request(struct theft *t,
    struct theft_autoshrink_env *env,
    const struct theft_autoshrink_bit_pool *orig,
    struct theft_autoshrink_bit_pool *pool);

static size_t offset_of_pos(const struct theft_autoshrink_bit_pool *orig,
    size_t pos);

static void convert_bit_offset(size_t bit_offset,
    size_t *byte_offset, uint8_t *bit);

static uint64_t
read_bits_at_offset(const struct theft_autoshrink_bit_pool *pool,
    size_t bit_offset, uint8_t size);

static void
write_bits_at_offset(struct theft_autoshrink_bit_pool *pool,
    size_t bit_offset, uint8_t size, uint64_t bits);

static void truncate_trailing_zero_bytes(struct theft_autoshrink_bit_pool *pool);

#endif
