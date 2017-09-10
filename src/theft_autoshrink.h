#ifndef THEFT_AUTOSHRINK_H
#define THEFT_AUTOSHRINK_H

#include "theft_types_internal.h"
#include <limits.h>

#include "theft_hash.h"
#include "theft_shrink.h"
#include "theft_autoshrink_model.h"

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

struct autoshrink_env {
    // config
    uint8_t arg_i;
    size_t pool_size;
    size_t pool_limit;
    enum theft_autoshrink_print_mode print_mode;
    size_t max_failed_shrinks;
    uint64_t drop_threshold;
    uint8_t drop_bits;

    struct autoshrink_model *model;
    struct autoshrink_bit_pool *bit_pool;

    // allow injecting a fake prng, for testing
    bool leave_trailing_zeroes;
    prng_fun *prng;
    void *udata;
};

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

/* Alloc callback, with autoshrink_env passed along. */
enum theft_alloc_res
theft_autoshrink_alloc(struct theft *t, struct autoshrink_env *env,
    void **instance);

theft_hash
theft_autoshrink_hash(struct autoshrink_env *env);

void
theft_autoshrink_print(struct theft *t, FILE *f,
    struct autoshrink_env *env, const void *instance, void *type_env);

bool
theft_autoshrink_make_candidate_bit_pool(struct theft *t,
    struct autoshrink_env *env,
    struct autoshrink_bit_pool **output_bit_pool);

enum shrink_res
theft_autoshrink_shrink(struct theft *t,
    struct autoshrink_env *env,
    uint32_t tactic, void **output,
    struct autoshrink_bit_pool **output_bit_pool);

/* This is only exported for testing. */
void theft_autoshrink_dump_bit_pool(FILE *f, size_t bit_count,
    const struct autoshrink_bit_pool *pool,
    const struct autoshrink_bit_pool *req_pool,
    enum theft_autoshrink_print_mode print_mode);

#endif
