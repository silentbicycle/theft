#ifndef THEFT_TYPES_INTERNAL_H
#define THEFT_TYPES_INTERNAL_H

#include "theft.h"

#define THEFT_MAX_TACTICS ((uint32_t)-1)
#define DEFAULT_THEFT_SEED 0xa600d64b175eedLL

struct theft {
    FILE *out;
    theft_seed seed;
    uint8_t requested_bloom_bits;
    struct theft_bloom *bloom;  /* bloom filter */

    struct theft_mt *mt;        /* random number generator */
    uint64_t prng_buf;          /* buffer for PRNG bits */
    uint8_t bits_available;
};

enum all_gen_res_t {
    ALL_GEN_OK,                 /* all arguments generated okay */
    ALL_GEN_SKIP,               /* skip due to user constraints */
    ALL_GEN_DUP,                /* skip probably duplicated trial */
    ALL_GEN_ERROR,              /* memory error or other failure */
} all_gen_res_t;

enum shrink_res {
    SHRINK_OK,                  /* simplified argument further */
    SHRINK_DEAD_END,            /* at local minima */
    SHRINK_ERROR,               /* hard error during shrinking */
    SHRINK_HALT,                /* don't shrink any further */
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

    /* Progress callback. */
    theft_hook_cb *hook_cb;
    /* User environment for all callbacks. */
    void *env;

    /* Counters passed to hook callback */
    size_t pass;
    size_t fail;
    size_t skip;
    size_t dup;
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
