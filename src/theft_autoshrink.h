#ifndef THEFT_AUTOSHRINK_H
#define THEFT_AUTOSHRINK_H

#include "theft_types_internal.h"

#define AUTOSHRINK_ENV_TAG 0xa5
#define AUTOSHRINK_BIT_POOL_TAG 'B'

struct theft_autoshrink_bit_pool {
    uint8_t tag;
    uint8_t *bits;
    size_t size;  // in bits, not bytes
    size_t consumed;
    void **instance;
};

struct theft_autoshrink_env {
    uint8_t tag;
    struct theft_type_info user_type_info;
};

bool theft_autoshrink_wrap(struct theft *t,
    struct theft_type_info *type_info, struct theft_type_info *wrapper);

struct theft_autoshrink_bit_pool *
theft_autoshrink_init_bit_pool(struct theft *t,
    size_t size, theft_seed seed);

void theft_autoshrink_free_bit_pool(struct theft *t,
    struct theft_autoshrink_bit_pool *pool);

uint64_t
theft_autoshrink_bit_pool_random(struct theft_autoshrink_bit_pool *pool,
    uint8_t bit_count);

void
theft_autoshrink_get_real_args(struct theft_run_info *run_info,
    void **dst, void **src);

#endif
