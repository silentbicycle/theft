#ifndef THEFT_TYPES_H
#define THEFT_TYPES_H

/* A pseudo-random number/seed, used to generate instances. */
typedef uint64_t theft_seed;

/* A hash of an instance. */
typedef uint64_t theft_hash;

/* These are opaque, as far as the API is concerned. */
struct theft_bloom;             /* bloom filter */
struct theft_mt;                /* mersenne twister PRNG */

/* Opaque struct handle for property-test runner. */
struct theft;

/* Allocate and return an instance of the type, based on a pseudo-random
 * number stream with a known seed. To get random numbers, use
 * theft_random(t) or theft_random_bits(t, bit_count); this stream of
 * numbers will be deterministic, so if the alloc callback is
 * constructed appropriately, an identical instance can be constructed
 * later from the same initial seed and environment.
 *
 * The allocated instance should be written into *instance. */
enum theft_alloc_res {
    THEFT_ALLOC_OK,
    THEFT_ALLOC_SKIP,
    THEFT_ALLOC_ERROR,
};
typedef enum theft_alloc_res
theft_alloc_cb(struct theft *t, void *env, void **instance);

/* Free an instance. */
typedef void
theft_free_cb(void *instance, void *env);

/* Hash an instance. Used to skip combinations of arguments which
 * have probably already been checked. */
typedef theft_hash
theft_hash_cb(const void *instance, void *env);

/* Attempt to shrink an instance to a simpler instance.
 *
 * For a given INSTANCE, there are likely to be multiple ways in which
 * it can be simplified. For example, a list of unsigned ints could have
 * the first element decremented, divided by 2, or dropped. This
 * callback should write a pointer to a freshly allocated, simplified
 * instance in *output, or should return THEFT_SHRINK_DEAD_END to
 * indicate that the instance cannot be simplified further by this
 * method.
 *
 * These tactics will be lazily explored breadth-first, to
 * try to find simpler versions of arguments that cause the
 * property to no longer hold.
 *
 * If there are no other tactics to try for this instance, then
 * return THEFT_SHRINK_NO_MORE_TACTICS. Otherwise, theft will
 * keep calling the callback with successive tactics.
 *
 * If this callback is NULL, it is equivalent to always returning
 * THEFT_SHRINK_NO_MORE_TACTICS. */
enum theft_shrink_res {
    THEFT_SHRINK_OK,
    THEFT_SHRINK_DEAD_END,
    THEFT_SHRINK_NO_MORE_TACTICS,
    THEFT_SHRINK_ERROR,
};
typedef enum theft_shrink_res
theft_shrink_cb(struct theft *t, const void *instance, uint32_t tactic,
    void *env, void **output);

/* Print INSTANCE to output stream F.
 * Used for displaying counter-examples. Can be NULL. */
typedef void
theft_print_cb(FILE *f, const void *instance, void *env);

/* Result from a single trial. */
enum theft_trial_res {
    THEFT_TRIAL_PASS,           /* property held */
    THEFT_TRIAL_FAIL,           /* property contradicted */
    THEFT_TRIAL_SKIP,           /* user requested skip; N/A */
    THEFT_TRIAL_DUP,            /* args probably already tried */
    THEFT_TRIAL_ERROR,          /* unrecoverable error, halt */
};

/* A test property function. Arguments must match the types specified by
 * theft_config.type_info, or the result will be undefined. For example,
 * a propfun `prop_foo(A x, B y, C z)` must have a type_info array of
 * `{ info_A, info_B, info_C }`.
 *
 * Should return:
 *     THEFT_TRIAL_PASS if the property holds,
 *     THEFT_TRIAL_FAIL if a counter-example is found,
 *     THEFT_TRIAL_SKIP if the combination of args isn't applicable,
 *  or THEFT_TRIAL_ERROR if the whole run should be halted. */
typedef enum theft_trial_res theft_propfun( /* arguments unconstrained */ );

/* When printing an autoshrink bit pool, should just the user's print
 * callback be used (if available), or should it also print the raw
 * bit pool and/or the request sizes and values? */
enum theft_autoshrink_print_mode {
    THEFT_AUTOSHRINK_PRINT_USER = 0x00,
    THEFT_AUTOSHRINK_PRINT_BIT_POOL = 0x01,
    THEFT_AUTOSHRINK_PRINT_REQUESTS = 0x02,
    THEFT_AUTOSHRINK_PRINT_ALL = 0x03,
};

/* Configuration for autoshrinking.
 * For all of these, leaving them as 0 will use the default. */
struct theft_autoshrink_config {
    bool enable;                /* true: Enable autoshrinking */
    size_t pool_size;           /* Initial allocation size */
    /* Max number of random bits usable per trial.
     * (Default: no limit) */
    size_t pool_limit;          /* Max number of random bits per trial */
    enum theft_autoshrink_print_mode print_mode;

    /* How many unsuccessful shrinking attempts to try in a row
     * before deciding a local minimum has been reached. */
    size_t max_failed_shrinks;
};

/* Callbacks used for testing with random instances of a type.
 * For more information, see comments on their typedefs. */
struct theft_type_info {
    /* Required: */
    theft_alloc_cb *alloc;      /* gen random instance from seed */

    /* Optional, but recommended: */
    theft_free_cb *free;        /* free instance */
    theft_hash_cb *hash;        /* instance -> hash */
    theft_shrink_cb *shrink;    /* shrink instance */
    theft_print_cb *print;      /* fprintf instance */

    struct theft_autoshrink_config autoshrink_config;

    /* Optional environment, passed to the callbacks above. */
    void *env;
};

/* Overall trial pass/fail/skip/duplicate counts after a run. */
struct theft_run_report {
    size_t pass;
    size_t fail;
    size_t skip;
    size_t dup;
};


/*********
 * Hooks *
 *********/

/* Pre-run hook: called before the start of a run (group of trials). */
enum theft_hook_run_pre_res {
    THEFT_HOOK_RUN_PRE_ERROR,
    THEFT_HOOK_RUN_PRE_CONTINUE,
};
struct theft_hook_run_pre_info {
    const char *prop_name;
    size_t total_trials;
    theft_seed run_seed;
};
typedef enum theft_hook_run_pre_res
theft_hook_run_pre_cb(const struct theft_hook_run_pre_info *info,
    void *env);

/* Post-run hook: called after the whole run has completed,
 * with overall results. */
enum theft_hook_run_post_res {
    THEFT_HOOK_RUN_POST_ERROR,
    THEFT_HOOK_RUN_POST_CONTINUE,
};
struct theft_hook_run_post_info {
    const char *prop_name;
    size_t total_trials;
    theft_seed run_seed;
    struct theft_run_report report;
};
typedef enum theft_hook_run_post_res
theft_hook_run_post_cb(const struct theft_hook_run_post_info *info,
    void *env);

/* Pre-argument generation hook: called before an individual trial's
 * argument(s) are generated. */
enum theft_hook_gen_args_pre_res {
    THEFT_HOOK_GEN_ARGS_PRE_ERROR,
    THEFT_HOOK_GEN_ARGS_PRE_CONTINUE,
    THEFT_HOOK_GEN_ARGS_PRE_HALT,
};
struct theft_hook_gen_args_pre_info {
    const char *prop_name;
    size_t total_trials;
    theft_seed run_seed;
    size_t trial_id;
    theft_seed trial_seed;
    uint8_t arity;
};
typedef enum theft_hook_gen_args_pre_res
theft_hook_gen_args_pre_cb(const struct theft_hook_gen_args_pre_info *info,
    void *env);

/* Pre-trial hook: called before running the trial, with the initially
 * generated argument(s). */
enum theft_hook_trial_pre_res {
    THEFT_HOOK_TRIAL_PRE_ERROR,
    THEFT_HOOK_TRIAL_PRE_CONTINUE,
    THEFT_HOOK_TRIAL_PRE_HALT,
};
struct theft_hook_trial_pre_info {
    const char *prop_name;
    size_t total_trials;
    theft_seed run_seed;
    size_t trial_id;
    theft_seed trial_seed;
    uint8_t arity;
    void **args;
};
typedef enum theft_hook_trial_pre_res
theft_hook_trial_pre_cb(const struct theft_hook_trial_pre_info *info,
    void *env);

/* Post-trial hook: called after the trial is run, with the arguments
 * and result.*/
enum theft_hook_trial_post_res {
    THEFT_HOOK_TRIAL_POST_ERROR,
    THEFT_HOOK_TRIAL_POST_CONTINUE,
    THEFT_HOOK_TRIAL_POST_REPEAT,
    THEFT_HOOK_TRIAL_POST_REPEAT_ONCE,
};
struct theft_hook_trial_post_info {
    const char *prop_name;
    size_t total_trials;
    theft_seed run_seed;
    size_t trial_id;
    theft_seed trial_seed;
    uint8_t arity;
    void **args;
    enum theft_trial_res result;
};
typedef enum theft_hook_trial_post_res
theft_hook_trial_post_cb(const struct theft_hook_trial_post_info *info,
    void *env);

/* Pre-shrinking hook: called before each shrinking attempt.
 * Returning HALT will keep shrinking from going any further. */
enum theft_hook_shrink_pre_res {
    THEFT_HOOK_SHRINK_PRE_ERROR,
    THEFT_HOOK_SHRINK_PRE_CONTINUE,
    THEFT_HOOK_SHRINK_PRE_HALT,
};
struct theft_hook_shrink_pre_info {
    const char *prop_name;
    size_t total_trials;
    theft_seed run_seed;
    size_t trial_id;
    theft_seed trial_seed;
    uint8_t arity;
    size_t shrink_count;
    size_t successful_shrinks;
    size_t failed_shrinks;
    uint8_t arg_index;
    void *arg;
    uint32_t tactic;
};
typedef enum theft_hook_shrink_pre_res
theft_hook_shrink_pre_cb(const struct theft_hook_shrink_pre_info *info,
    void *env);

/* Post-shrinking hook: called after attempting to shrink. */
enum theft_hook_shrink_post_res {
    THEFT_HOOK_SHRINK_POST_ERROR,
    THEFT_HOOK_SHRINK_POST_CONTINUE,
};
struct theft_hook_shrink_post_info {
    const char *prop_name;
    size_t total_trials;
    theft_seed run_seed;
    size_t trial_id;
    theft_seed trial_seed;
    uint8_t arity;
    size_t shrink_count;
    size_t successful_shrinks;
    size_t failed_shrinks;
    uint8_t arg_index;
    void *arg;
    uint32_t tactic;
    /* Did shrink() indicate that we're at a local minimum? */
    bool done;
};
typedef enum theft_hook_shrink_post_res
theft_hook_shrink_post_cb(const struct theft_hook_shrink_post_info *info,
    void *env);

/* Post-trial-shrinking hook: called after running a trial with
 * shrunken arguments. Returning REPEAT will run the trial again
 * with the same argument(s). */
enum theft_hook_shrink_trial_post_res {
    THEFT_HOOK_SHRINK_TRIAL_POST_ERROR,
    THEFT_HOOK_SHRINK_TRIAL_POST_CONTINUE,
    THEFT_HOOK_SHRINK_TRIAL_POST_REPEAT,
    THEFT_HOOK_SHRINK_TRIAL_POST_REPEAT_ONCE,
};
struct theft_hook_shrink_trial_post_info {
    const char *prop_name;
    size_t total_trials;
    theft_seed run_seed;
    size_t trial_id;
    theft_seed trial_seed;
    uint8_t arity;
    size_t shrink_count;
    size_t successful_shrinks;
    size_t failed_shrinks;
    uint8_t arg_index;
    void **args;
    uint32_t tactic;
    enum theft_trial_res result;
};
typedef enum theft_hook_shrink_trial_post_res
theft_hook_shrink_trial_post_cb(const struct theft_hook_shrink_trial_post_info *info,
    void *env);


/*****************
 * Configuration *
 *****************/

/* Result from a trial run. */
enum theft_run_res {
    THEFT_RUN_PASS = 0,             /* no failures */
    THEFT_RUN_FAIL = 1,             /* 1 or more failures */
    THEFT_RUN_ERROR = 2,            /* an error occurred */
    THEFT_RUN_ERROR_BAD_ARGS = -1,  /* API misuse */
    /* Missing required callback for 1 or more types */
    THEFT_RUN_ERROR_MISSING_CALLBACK = -2,
};

/* Default number of trials to run. */
#define THEFT_DEF_TRIALS 100

/* Min and max bits used to determine bloom filter size.
 * (A larger value uses more memory, but reduces the odds of an
 * untested argument combination being falsely skipped.) */
#define THEFT_BLOOM_BITS_MIN 13 /* 1 KB */
#define THEFT_BLOOM_BITS_MAX 33 /* 1 GB */

/* Configuration struct for a theft run. */
struct theft_run_config {
    /* Property function under test, and info about its arguments.
     * The function is called with as many arguments are there
     * are values in TYPE_INFO, so it can crash if that is wrong. */
    theft_propfun *fun;
    struct theft_type_info *type_info[THEFT_MAX_ARITY];

    /* -- All fields after this point are optional. -- */

    /* Property name, displayed in test runner output. */
    const char *name;

    /* Array of seeds to always run, and its length.
     * Can be used for regression tests. */
    size_t always_seed_count;      /* number of seeds */
    theft_seed *always_seeds;      /* seeds to always run */

    /* Number of trials to run. Defaults to THEFT_DEF_TRIALS. */
    size_t trials;

    /* The number of bits to use for the bloom filter, which
     * detects combinations of arguments that have already
     * been tested. If 0, a default size will be chosen
     * based on the trial count. (This will only be used if
     * all property types have hash callbacks defined.) */
    uint8_t bloom_bits;

    /* These functions are called in several contexts to report on
     * progress, halt shrinking early, repeat trials with different
     * logging, etc.
     *
     * See their function pointer typedefs above for details. */
    struct {
        theft_hook_run_pre_cb *run_pre;
        theft_hook_run_post_cb *run_post;
        theft_hook_gen_args_pre_cb *gen_args_pre;
        theft_hook_trial_pre_cb *trial_pre;
        theft_hook_trial_post_cb *trial_post;
        theft_hook_shrink_pre_cb *shrink_pre;
        theft_hook_shrink_post_cb *shrink_post;
        theft_hook_shrink_trial_post_cb *shrink_trial_post;
        /* Environment pointer. This is completely opaque to theft
         * itself, but will be passed to all callbacks. */
        void *env;
    } hooks;

    /* Struct to populate with more detailed test results. */
    struct theft_trial_report *report;

    /* Seed for the random number generator. */
    theft_seed seed;
};

/* Internal state for incremental hashing. */
struct theft_hasher {
    theft_hash accum;
};

#endif
