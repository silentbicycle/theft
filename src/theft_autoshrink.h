#ifndef THEFT_AUTOSHRINK_H
#define THEFT_AUTOSHRINK_H

#include "theft_types_internal.h"
#include <limits.h>

#define AUTOSHRINK_ENV_TAG 0xa5
#define AUTOSHRINK_BIT_POOL_TAG 'B'

struct autoshrink_bit_pool {
    /* Bits will always be rounded up to a multiple of 64 bits,
     * and be aligned as a uint64_t. */
    uint8_t *bits;
    bool shrinking;             /* is this pool shrinking? */
    size_t bits_filled;         /* how many bits are available */
    size_t bits_ceil;           /* ceiling for bit buffer */
    size_t limit;               /* after limit bytes, return 0 */

    size_t consumed;
    size_t request_count;
    size_t request_ceil;
    uint32_t *requests;

    size_t generation;
    size_t *index;
};

/* How large should the default autoshrink bit pool be?
 * The pool will be filled and grown on demand, but an
 * excessively small initial pool will lead to several
 * reallocs in quick succession. */
#define DEF_POOL_SIZE (64 * 8*sizeof(uint64_t))

/* How large should the buffer for request sizes be by default? */
#define DEF_REQUESTS_CEIL2 4 /* constrain to a power of 2 */
#define DEF_REQUESTS_CEIL (1 << DEF_REQUESTS_CEIL2)

/* Default: Decide we've reached a local minimum after
 * this many unsuccessful shrinks in a row. */
#define DEF_MAX_FAILED_SHRINKS 100

/* When attempting to drop records, default to odds of
 * (1+DEF_DROP_THRESHOLD) in (1 << DEF_DROP_BITS). */
#define DEF_DROP_THRESHOLD 0
#define DEF_DROP_BITS 5

/* Max number of pooled random bits to give to alloc callback
 * before returning 0 forever. Default: No limit. */
#define DEF_POOL_LIMIT ULLONG_MAX

/* Magic value to disable selecting a request to drop in
 * drop_from_bit_pool, because it complicates tests. */
#define DO_NOT_DROP (0xFFFFFFFFLU)

typedef uint64_t autoshrink_prng_fun(uint8_t bits, void *udata);

#define TWO_EVENLY 0x80
#define FOUR_EVENLY 0x40
#define MODEL_MIN 0x08
#define MODEL_MAX 0x80

#define DROPS_MIN 0x10
#define DROPS_MAX 0xA0

enum autoshrink_action {
    ASA_DROP = 0x01,
    ASA_SHIFT = 0x02,
    ASA_MASK = 0x04,
    ASA_SWAP = 0x08,
    ASA_SUB = 0x10,
};

enum autoshrink_weight {
    WEIGHT_DROP = 0x00,
    WEIGHT_SHIFT = 0x01,
    WEIGHT_MASK = 0x02,
    WEIGHT_SWAP = 0x03,
    WEIGHT_SUB = 0x04,
};

struct autoshrink_model {
    enum autoshrink_action cur_tried;
    enum autoshrink_action cur_set;
    enum autoshrink_action next_action;
    uint8_t weights[5];
};

struct autoshrink_env {
    // config
    uint8_t arg_i;
    size_t pool_size;
    size_t pool_limit;
    enum theft_autoshrink_print_mode print_mode;
    size_t max_failed_shrinks;
    uint64_t drop_threshold;
    uint8_t drop_bits;

    struct autoshrink_model model;
    struct autoshrink_bit_pool *bit_pool;

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

struct autoshrink_env *
theft_autoshrink_alloc_env(struct theft *t, uint8_t arg_i,
    const struct theft_type_info *type_info);

void theft_autoshrink_free_env(struct theft *t, struct autoshrink_env *env);

enum theft_autoshrink_wrap {
    THEFT_AUTOSHRINK_WRAP_OK,
    THEFT_AUTOSHRINK_WRAP_ERROR_MEMORY = -1,
    THEFT_AUTOSHRINK_WRAP_ERROR_MISUSE = -2,
};
enum theft_autoshrink_wrap
theft_autoshrink_wrap(struct theft *t,
    struct theft_type_info *type_info, struct theft_type_info *wrapper);

void theft_autoshrink_free_bit_pool(struct theft *t,
    struct autoshrink_bit_pool *pool);

void
theft_autoshrink_bit_pool_random(struct theft *t,
    struct autoshrink_bit_pool *pool,
    uint32_t bit_count, bool save_request,
    uint64_t *buf);

void
theft_autoshrink_get_real_args(struct theft *t,
    void **dst, void **src);

void
theft_autoshrink_update_model(struct theft *t,
    uint8_t arg_id, enum theft_trial_res res,
    uint8_t adjustment);

/* Alloc callback, with autoshrink_env passed along. */
enum theft_alloc_res
theft_autoshrink_alloc(struct theft *t, struct autoshrink_env *env,
    void **instance);

theft_hash
theft_autoshrink_hash(struct theft *t, const void *instance,
    struct autoshrink_env *env, void *type_env);

void
theft_autoshrink_print(struct theft *t, FILE *f,
    struct autoshrink_env *env, const void *instance, void *type_env);

enum theft_shrink_res
theft_autoshrink_shrink(struct theft *t,
    struct autoshrink_env *env,
    uint32_t tactic, void **output,
    struct autoshrink_bit_pool **output_bit_pool);

/* This is only exported for testing. */
void theft_autoshrink_dump_bit_pool(FILE *f, size_t bit_count,
    const struct autoshrink_bit_pool *pool,
    enum theft_autoshrink_print_mode print_mode);

/* Set the next action the model will deliver. (This is a hook for testing.) */
void theft_autoshrink_model_set_next(struct autoshrink_env *env,
    enum autoshrink_action action);

#endif
