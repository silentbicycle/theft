#ifndef THEFT_TYPES_INTERNAL_H
#define THEFT_TYPES_INTERNAL_H

#include "theft.h"
#include <inttypes.h>
#include <string.h>

#define THEFT_MAX_TACTICS ((uint32_t)-1)
#define DEFAULT_THEFT_SEED 0xa600d64b175eedLL

#define THEFT_LOG_LEVEL 0
#define LOG(LEVEL, ...)                                               \
    do {                                                              \
        if (LEVEL <= THEFT_LOG_LEVEL) {                               \
            printf(__VA_ARGS__);                                      \
        }                                                             \
    } while(0)

struct theft_bloom;             /* bloom filter */
struct theft_mt;                /* mersenne twister PRNG */

struct theft {
    FILE *out;
    theft_seed seed;
    uint8_t requested_bloom_bits;
    struct theft_bloom *bloom;  /* bloom filter */

    struct theft_mt *mt;        /* random number generator */
    uint64_t prng_buf;          /* buffer for PRNG bits */
    uint8_t bits_available;
    /* Bit pool, only used during autoshrinking. */
    struct theft_autoshrink_bit_pool *bit_pool;
    struct theft_run_info *run_info;

    /* Counters passed to hook callback */
    struct {
        size_t pass;
        size_t fail;
        size_t skip;
        size_t dup;
    } counters;
};

/* Testing context for a specific property function. */
struct theft_run_info {
    const char *name;           /* property name, can be NULL */
    theft_propfun * const fun;  /* property function under test */
    const size_t trial_count;
    const theft_seed run_seed;

    /* Type info for ARITY arguments. */
    const uint8_t arity;        /* number of arguments */
    struct theft_type_info *type_info[THEFT_MAX_ARITY];

    /* Optional array of seeds to always run.
     * Can be used for regression tests. */
    const size_t always_seed_count;   /* number of seeds */
    const theft_seed *always_seeds;   /* seeds to always run */

    /* Progress callbacks. */
    struct {
        theft_hook_run_pre_cb *run_pre;
        theft_hook_run_post_cb *run_post;
        theft_hook_gen_args_pre_cb *gen_args_pre;
        theft_hook_trial_pre_cb *trial_pre;
        theft_hook_trial_post_cb *trial_post;
        theft_hook_counterexample_cb *counterexample;
        theft_hook_shrink_pre_cb *shrink_pre;
        theft_hook_shrink_post_cb *shrink_post;
        theft_hook_shrink_trial_post_cb *shrink_trial_post;
        void *env;
    } hooks;

    struct {
        bool enable;
        size_t timeout;
        int signal;
    } fork;

    struct theft_print_trial_result_env *print_trial_result_env;
};

/* Result from an individual trial. */
struct theft_trial_info {
    const int trial;            /* N'th trial */
    theft_seed seed;            /* Seed used */
    enum theft_trial_res status;   /* Run status */
    const uint8_t arity;        /* Number of arguments */
    void **args;                /* Arguments used */
    size_t shrink_count;
    size_t successful_shrinks;
    size_t failed_shrinks;
};

#endif
