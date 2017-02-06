#ifndef THEFT_AUTOSHRINK_INTERNAL_H
#define THEFT_AUTOSHRINK_INTERNAL_H

/* FIXME: make this configurable and determine a reasonable default */
#define DEF_POOL_SIZE (32 * 1024LU)

#include "theft_types_internal.h"
#include "theft_autoshrink.h"

#include <assert.h>

static theft_alloc_cb autoshrink_alloc;
static theft_free_cb autoshrink_free;
static theft_hash_cb autoshrink_hash;
static theft_shrink_cb autoshrink_shrink;
static theft_print_cb autoshrink_print;

static struct theft_autoshrink_bit_pool *alloc_bit_pool(size_t size);

#define MAX_AUTOSHRINKS 256

static enum theft_alloc_res
alloc_from_bit_pool(struct theft *t, struct theft_autoshrink_env *env,
    struct theft_autoshrink_bit_pool *bit_pool, void **output);

static void drop_bits(struct theft *t, uint8_t bits,
    struct theft_autoshrink_bit_pool *dst,
    const struct theft_autoshrink_bit_pool *src);

static void mask_bits(struct theft *t, uint8_t bits,
    struct theft_autoshrink_bit_pool *dst,
    const struct theft_autoshrink_bit_pool *src);

#endif
