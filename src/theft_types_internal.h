#ifndef THEFT_TYPES_INTERNAL_H
#define THEFT_TYPES_INTERNAL_H

#include "theft.h"
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>

#define THEFT_MAX_TACTICS ((uint32_t)-1)
#define DEFAULT_THEFT_SEED 0xa600d64b175eedLLU

#define THEFT_LOG_LEVEL 0
#define LOG(LEVEL, ...)                                               \
    do {                                                              \
        if (LEVEL <= THEFT_LOG_LEVEL) {                               \
            printf(__VA_ARGS__);                                      \
        }                                                             \
    } while(0)

struct theft_bloom;             /* bloom filter */
struct theft_rng;               /* pseudorandom number generator */

struct seed_info {
    const theft_seed run_seed;

    /* Optional array of seeds to always run.
     * Can be used for regression tests. */
    const size_t always_seed_count;   /* number of seeds */
    const theft_seed *always_seeds;   /* seeds to always run */
};

struct fork_info {
    const bool enable;
    const size_t timeout;
    const int signal;
    const size_t exit_timeout;
};

struct prop_info {
    const char *name;           /* property name, can be NULL */
    /* property function under test */
    union {
        theft_propfun1 *fun1;
        theft_propfun2 *fun2;
        theft_propfun3 *fun3;
        theft_propfun4 *fun4;
        theft_propfun5 *fun5;
        theft_propfun6 *fun6;
        theft_propfun7 *fun7;
    } u;
    const size_t trial_count;

    /* Type info for ARITY arguments. */
    const uint8_t arity;        /* number of arguments */
    struct theft_type_info *type_info[THEFT_MAX_ARITY];
};

struct hook_info {
    theft_hook_run_pre_cb *run_pre;
    theft_hook_run_post_cb *run_post;
    theft_hook_gen_args_pre_cb *gen_args_pre;
    theft_hook_trial_pre_cb *trial_pre;
    theft_hook_fork_post_cb *fork_post;
    theft_hook_trial_post_cb *trial_post;
    theft_hook_counterexample_cb *counterexample;
    theft_hook_shrink_pre_cb *shrink_pre;
    theft_hook_shrink_post_cb *shrink_post;
    theft_hook_shrink_trial_post_cb *shrink_trial_post;
    void *env;
};

struct counter_info {
    size_t pass;
    size_t fail;
    size_t skip;
    size_t dup;
};

struct prng_info {
    struct theft_rng *rng;      /* random number generator */
    uint64_t buf;               /* buffer for PRNG bits */
    uint8_t bits_available;
    /* Bit pool, only used during autoshrinking. */
    struct autoshrink_bit_pool *bit_pool;
};

enum arg_type {
    ARG_BASIC,
    ARG_AUTOSHRINK,
};

struct arg_info {
    void *instance;

    enum arg_type type;
    union {
        struct {
            struct autoshrink_env *env;
        } as;
    } u;
};

/* Result from an individual trial. */
struct trial_info {
    const int trial;               /* N'th trial */
    theft_seed seed;               /* Seed used */
    size_t shrink_count;
    size_t successful_shrinks;
    size_t failed_shrinks;
    struct arg_info args[THEFT_MAX_ARITY];
};

enum worker_state {
    WS_INACTIVE,
    WS_ACTIVE,
    WS_STOPPED,
};

struct worker_info {
    enum worker_state state;
    int fds[2];
    pid_t pid;
    int wstatus;
};

/* Handle to state for the entire run. */
struct theft {
    FILE *out;
    struct theft_bloom *bloom;  /* bloom filter */
    struct theft_print_trial_result_env *print_trial_result_env;

    struct prng_info prng;
    struct prop_info prop;
    struct seed_info seeds;
    struct fork_info fork;
    struct hook_info hooks;
    struct counter_info counters;
    struct trial_info trial;
    struct worker_info workers[1];
};

#endif
