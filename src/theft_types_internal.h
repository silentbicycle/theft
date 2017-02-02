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
};

/* Testing context for a specific property function. */
struct theft_propfun_info {
    const char *name;           /* property name, can be NULL */
    theft_propfun *fun;         /* property function under test */

    /* Type info for ARITY arguments. */
    uint8_t arity;              /* number of arguments */
    struct theft_type_info *type_info[THEFT_MAX_ARITY];

    /* Optional array of seeds to always run.
     * Can be used for regression tests. */
    int always_seed_count;      /* number of seeds */
    theft_seed *always_seeds;   /* seeds to always run */
};

#endif
