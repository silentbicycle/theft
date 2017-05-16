#ifndef THEFT_AUTOSHRINK_H
#define THEFT_AUTOSHRINK_H

#include "theft_types_internal.h"

#define AUTOSHRINK_ENV_TAG 0xa5
#define AUTOSHRINK_BIT_POOL_TAG 'B'

struct theft_autoshrink_bit_pool {
    uint8_t tag;

    /* Bits will always be rounded up to a multiple of 64 bits,
     * and be aligned as a uint64_t. */
    uint8_t *bits;
    size_t size;  // in bits, not bytes

    /* The most recently generated instance, if any. */
    void **instance;

    size_t consumed;
    size_t request_count;
    size_t request_ceil;
    // TODO: should this be uint32_t or uint16_t?
    uint32_t *requests;
};


#define DEF_POOL_SIZE (32 * 1024LU)

/* FIXME: make this configurable and determine a reasonable default */
#define DEF_REQUESTS_CEIL2 4
#define DEF_REQUESTS_CEIL (1 << DEF_REQUESTS_CEIL2)
#define DEF_MAX_FAILED_SHRINKS 100
#define DEF_DROP_TACTICS 8

/* Default: 1/32 odds of dropping */
#define DEF_DROP_THRESHOLD 1
#define DEF_DROP_BITS 5

/* Magic value to disable selecting a request to drop in
 * drop_from_bit_pool, because it complicates tests. */
#define DO_NOT_DROP (0xFFFFFFFFLU)

typedef uint64_t autoshrink_prng_fun(uint8_t bits, void *udata);

struct theft_autoshrink_env {
    uint8_t tag;
    struct theft_type_info user_type_info;

    // config
    size_t pool_size;
    enum theft_autoshrink_print_mode print_mode;
    size_t max_failed_shrinks;
    uint32_t max_autoshrinks;  // FIXME: name
    uint32_t drop_tactics;     // FIXME: name
    uint64_t drop_threshold;   // FIXME: name
    uint8_t drop_bits;         // FIXME: name

    // allow injecting a fake prng, for testing
    bool leave_trailing_zeroes;
    autoshrink_prng_fun *prng;
    void *udata;
};

enum mutation {
    MUT_SHIFT,
    MUT_MASK,
    MUT_SWAP,
    MUT_SUB,
};
#define LAST_MUTATION MUT_SUB
#define MUTATION_TYPE_BITS 2

struct change_info {
    enum mutation t;
    size_t pos;
    uint32_t size;
    union {
        uint8_t shift;
        uint64_t mask;
        uint64_t and;
        uint64_t sub;
        uint8_t swap_unused;
    } u;
};

bool theft_autoshrink_wrap(struct theft *t,
    struct theft_type_info *type_info, struct theft_type_info *wrapper);

/* struct theft_autoshrink_bit_pool *
 * theft_autoshrink_init_bit_pool(struct theft *t,
 *     size_t size, theft_seed seed); */

void theft_autoshrink_free_bit_pool(struct theft *t,
    struct theft_autoshrink_bit_pool *pool);

uint64_t
theft_autoshrink_bit_pool_random(struct theft_autoshrink_bit_pool *pool,
    uint8_t bit_count, bool save_request);

void
theft_autoshrink_get_real_args(struct theft_run_info *run_info,
    void **dst, void **src);

/* These are only exported for testing. */
enum theft_shrink_res
theft_autoshrink_shrink(struct theft *t,
    const struct theft_autoshrink_bit_pool *orig, uint32_t tactic,
    struct theft_autoshrink_env *env, void **output);
void theft_autoshrink_dump_bit_pool(FILE *f, size_t bit_count,
    const struct theft_autoshrink_bit_pool *pool,
    enum theft_autoshrink_print_mode print_mode);

#endif
