#ifndef THEFT_TYPES_H
#define THEFT_TYPES_H

/* Opaque handle struct for a theft property-test runner. */
struct theft;

/* A pseudo-random number/seed, used to generate instances. */
typedef uint64_t theft_seed;

/* A hash of an instance. */
typedef uint64_t theft_hash;

/* Configuration for a theft run. (Forward reference, defined below.) */
struct theft_run_config;

/* Overall trial pass/fail/skip/duplicate counts after a run. */
struct theft_run_report {
    size_t pass;
    size_t fail;
    size_t skip;
    size_t dup;
};

/* Result from a single trial. */
enum theft_trial_res {
    THEFT_TRIAL_PASS,           /* property held */
    THEFT_TRIAL_FAIL,           /* property contradicted */
    THEFT_TRIAL_SKIP,           /* user requested skip; N/A */
    THEFT_TRIAL_DUP,            /* args probably already tried */
    THEFT_TRIAL_ERROR,          /* unrecoverable error, halt */
};

/* Result from a trial run (group of trials). */
enum theft_run_res {
    THEFT_RUN_PASS = 0,             /* no failures */
    THEFT_RUN_FAIL = 1,             /* 1 or more failures */
    THEFT_RUN_SKIP = 2,             /* no failures, but no passes either */
    THEFT_RUN_ERROR = 3,            /* an error occurred */
    THEFT_RUN_ERROR_MEMORY = -1,    /* memory allocation failure */
    THEFT_RUN_ERROR_BAD_ARGS = -2,  /* API misuse */
};

/* Result from generating and printing an instance based on a seed. */
enum theft_generate_res {
    THEFT_GENERATE_OK = 0,
    THEFT_GENERATE_SKIP = 1,
    THEFT_GENERATE_ERROR_ALLOC = -1, /* error in alloc cb */
    THEFT_GENERATE_ERROR_MEMORY = -2,
    THEFT_GENERATE_ERROR_BAD_ARGS = -3,
};

/* A test property function.
 * The argument count should match the number of callback structs
 * provided in `theft_config.type_info`.
 *
 * Should return:
 *     THEFT_TRIAL_PASS if the property holds,
 *     THEFT_TRIAL_FAIL if a counter-example is found,
 *     THEFT_TRIAL_SKIP if the combination of args isn't applicable,
 *  or THEFT_TRIAL_ERROR if the whole run should be halted. */
typedef enum theft_trial_res theft_propfun1(struct theft *t,
    void *arg1);
typedef enum theft_trial_res theft_propfun2(struct theft *t,
    void *arg1, void *arg2);
typedef enum theft_trial_res theft_propfun3(struct theft *t,
    void *arg1, void *arg2, void *arg3);
typedef enum theft_trial_res theft_propfun4(struct theft *t,
    void *arg1, void *arg2, void *arg3, void *arg4);
typedef enum theft_trial_res theft_propfun5(struct theft *t,
    void *arg1, void *arg2, void *arg3, void *arg4, void *arg5);
typedef enum theft_trial_res theft_propfun6(struct theft *t,
    void *arg1, void *arg2, void *arg3, void *arg4, void *arg5,
    void *arg6);
typedef enum theft_trial_res theft_propfun7(struct theft *t,
    void *arg1, void *arg2, void *arg3, void *arg4, void *arg5,
    void *arg6, void *arg7);

/* Internal state for incremental hashing. */
struct theft_hasher {
    theft_hash accum;
};


/***********************
 * Type info callbacks *
 ***********************/

/* This struct contains callbacks used to specify how to allocate, free,
 * hash, print, and/or shrink the property test input.
 *
 * Only `alloc` is required, though `free` is strongly recommended. */
struct theft_type_info;         /* (forward reference) */

/* Allocate and return an instance of the type, based on a pseudo-random
 * number stream with a known seed. To get random numbers, use
 * `theft_random_bits(t, bit_count)` or `theft_random_bits_bulk(t,
 * bit_count, buffer)`. This stream of numbers will be deterministic, so
 * if the alloc callback is constructed appropriately, an identical
 * instance can be constructed later from the same initial seed and
 * environment.
 *
 * The allocated instance should be written into *output.
 *
 * If autoshrinking is used, then alloc has an additional requirement:
 * getting smaller values from `theft_random_bits` should correspond to
 * simpler instances. In particular, if `theft_random_bits` returns 0
 * forever, alloc must generate a minimal instance. */
enum theft_alloc_res {
    THEFT_ALLOC_OK,
    THEFT_ALLOC_SKIP,
    THEFT_ALLOC_ERROR,
};
typedef enum theft_alloc_res
theft_alloc_cb(struct theft *t, void *env, void **output);

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
theft_shrink_cb(struct theft *t, const void *instance,
    uint32_t tactic, void *env, void **output);

/* Print INSTANCE to output stream F.
 * Used for displaying counter-examples. Can be NULL. */
typedef void
theft_print_cb(FILE *f, const void *instance, void *env);

/* When printing an autoshrink bit pool, should just the user's print
 * callback be used (if available), or should it also print the raw
 * bit pool and/or the request sizes and values? */
enum theft_autoshrink_print_mode {
    THEFT_AUTOSHRINK_PRINT_DEFAULT = 0x00,
    THEFT_AUTOSHRINK_PRINT_USER = 0x01,
    THEFT_AUTOSHRINK_PRINT_BIT_POOL = 0x02,
    THEFT_AUTOSHRINK_PRINT_REQUESTS = 0x04,
    THEFT_AUTOSHRINK_PRINT_ALL = 0x07,
};

/* Configuration for autoshrinking.
 * For all of these fields, leaving them as 0 will use the default. */
struct theft_autoshrink_config {
    bool enable;                /* true: Enable autoshrinking */
    /* Initial allocation size (default: DEF_POOL_SIZE).
     * When generating very complex instances, this may need to be increased. */
    size_t pool_size;
    enum theft_autoshrink_print_mode print_mode;

    /* How many unsuccessful shrinking attempts to try in a row
     * before deciding a local minimum has been reached.
     * Default: DEF_MAX_FAILED_SHRINKS. */
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
    theft_print_cb *print;      /* fprintf instance */
    /* shrink instance, if autoshrinking is not in use */
    theft_shrink_cb *shrink;

    struct theft_autoshrink_config autoshrink_config;

    /* Optional environment, passed to the callbacks above.
     * This is completely opaque to theft. */
    void *env;
};


/*********
 * Hooks *
 *********/

/* Much of theft's runtime behavior can be customized using these
 * hooks. In all cases, returning `*_ERROR` will cause theft to
 * halt everything, clean up, and return `THEFT_RUN_ERROR`. */


/* Pre-run hook: called before the start of a run (group of trials). */
enum theft_hook_run_pre_res {
    THEFT_HOOK_RUN_PRE_ERROR,
    THEFT_HOOK_RUN_PRE_CONTINUE,
};
struct theft_hook_run_pre_info {
    const char *prop_name;
    size_t total_trials;        /* total number of trials */
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
    /* Don't run any more trials (e.g. stop after N failures). */
    THEFT_HOOK_GEN_ARGS_PRE_HALT,
};
struct theft_hook_gen_args_pre_info {
    const char *prop_name;
    size_t total_trials;
    size_t trial_id;
    size_t failures;            /* failures so far */
    theft_seed run_seed;
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
    /* Don't run any more trials (e.g. stop after N failures). */
    THEFT_HOOK_TRIAL_PRE_HALT,
};
struct theft_hook_trial_pre_info {
    const char *prop_name;
    size_t total_trials;
    size_t trial_id;
    size_t failures;
    theft_seed run_seed;
    theft_seed trial_seed;
    uint8_t arity;
    void **args;
};
typedef enum theft_hook_trial_pre_res
theft_hook_trial_pre_cb(const struct theft_hook_trial_pre_info *info,
    void *env);

/* Post-fork hook: called on the child process after forking. */
enum theft_hook_fork_post_res {
    THEFT_HOOK_FORK_POST_ERROR,
    THEFT_HOOK_FORK_POST_CONTINUE,
};
struct theft_hook_fork_post_info {
    struct theft *t;
    const char *prop_name;
    size_t total_trials;
    size_t failures;
    theft_seed run_seed;
    uint8_t arity;
    void **args;
};
typedef enum theft_hook_fork_post_res
theft_hook_fork_post_cb(const struct theft_hook_fork_post_info *info,
    void *env);

/* Post-trial hook: called after the trial is run, with the arguments
 * and result. */
enum theft_hook_trial_post_res {
    THEFT_HOOK_TRIAL_POST_ERROR,
    THEFT_HOOK_TRIAL_POST_CONTINUE,
    /* Run the trial again with the same arguments. */
    THEFT_HOOK_TRIAL_POST_REPEAT,
    /* Same as REPEAT, but only repeat once. */
    THEFT_HOOK_TRIAL_POST_REPEAT_ONCE,
};
struct theft_hook_trial_post_info {
    struct theft *t;
    const char *prop_name;
    size_t total_trials;
    size_t trial_id;
    size_t failures;
    theft_seed run_seed;
    theft_seed trial_seed;
    uint8_t arity;
    void **args;
    enum theft_trial_res result;
    bool repeat;
};
typedef enum theft_hook_trial_post_res
theft_hook_trial_post_cb(const struct theft_hook_trial_post_info *info,
    void *env);

/* Counter-example hook: called when theft finds a counter-example
 * that causes a property test to fail.
 *
 * By default, this just calls `theft_print_counterexample`, but can
 * be overridden to log the counterexample some other way. */
enum theft_hook_counterexample_res {
    THEFT_HOOK_COUNTEREXAMPLE_CONTINUE,
    THEFT_HOOK_COUNTEREXAMPLE_ERROR,
};
struct theft_hook_counterexample_info {
    struct theft *t;
    const char *prop_name;
    size_t total_trials;
    size_t trial_id;
    theft_seed trial_seed;
    uint8_t arity;
    struct theft_type_info **type_info;
    void **args;
};
typedef enum theft_hook_counterexample_res
theft_hook_counterexample_cb(const struct theft_hook_counterexample_info *info,
    void *env);

/* Pre-shrinking hook: called before each shrinking attempt. */
enum theft_hook_shrink_pre_res {
    THEFT_HOOK_SHRINK_PRE_ERROR,
    THEFT_HOOK_SHRINK_PRE_CONTINUE,
    /* Don't attempt to shrink any further (e.g. if the user callback
     * checks a time limit). */
    THEFT_HOOK_SHRINK_PRE_HALT,
};
struct theft_hook_shrink_pre_info {
    const char *prop_name;
    size_t total_trials;
    size_t trial_id;
    size_t failures;
    theft_seed run_seed;
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

/* Post-shrinking hook: called after attempting to shrink, with
 * the new instance (if shrinking succeeded). */
enum theft_hook_shrink_post_res {
    THEFT_HOOK_SHRINK_POST_ERROR,
    THEFT_HOOK_SHRINK_POST_CONTINUE,
};
enum theft_shrink_post_state {
    THEFT_SHRINK_POST_SHRINK_FAILED,
    THEFT_SHRINK_POST_SHRUNK,
    THEFT_SHRINK_POST_DONE_SHRINKING,
};
struct theft_hook_shrink_post_info {
    const char *prop_name;
    size_t total_trials;
    size_t trial_id;
    theft_seed run_seed;
    theft_seed trial_seed;
    uint8_t arity;
    size_t shrink_count;
    size_t successful_shrinks;
    size_t failed_shrinks;
    uint8_t arg_index;
    void *arg;
    uint32_t tactic;
    /* Did this shrinking attempt make any progress?
     * If not, is shrinking done overall? */
    enum theft_shrink_post_state state;
};
typedef enum theft_hook_shrink_post_res
theft_hook_shrink_post_cb(const struct theft_hook_shrink_post_info *info,
    void *env);

/* Post-trial-shrinking hook: called after running a trial with
 * shrunken arguments. */
enum theft_hook_shrink_trial_post_res {
    THEFT_HOOK_SHRINK_TRIAL_POST_ERROR,
    THEFT_HOOK_SHRINK_TRIAL_POST_CONTINUE,
    /* Run the trial again, with the same argument(s). */
    THEFT_HOOK_SHRINK_TRIAL_POST_REPEAT,
    /* Same as REPEAT, but only repeat once. */
    THEFT_HOOK_SHRINK_TRIAL_POST_REPEAT_ONCE,
};
struct theft_hook_shrink_trial_post_info {
    const char *prop_name;
    size_t total_trials;
    size_t trial_id;
    size_t failures;
    theft_seed run_seed;
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

/* Should the floating-point generators be built? */
#ifndef THEFT_USE_FLOATING_POINT
#define THEFT_USE_FLOATING_POINT 1
#endif

/* Default number of trials to run. */
#define THEFT_DEF_TRIALS 100

/* Min and max bits used to determine bloom filter size.
 * (A larger value uses more memory, but reduces the odds of an
 * untested argument combination being falsely skipped.)
 *
 * These constants are no longer used, and will be removed
 * in a future release.*/
#define THEFT_BLOOM_BITS_MIN 13 /* 1 KB */
#define THEFT_BLOOM_BITS_MAX 33 /* 1 GB */

/* Default number of columns after which `theft_print_trial_result`
 * should wrap. */
#define THEFT_DEF_MAX_COLUMNS 72

/* A property can have at most this many arguments. */
#define THEFT_MAX_ARITY 7

/* For worker processes that were sent a timeout signal,
 * how long should they be given to terminate and exit
 * before sending kill(pid, SIGKILL). */
#define THEFT_DEF_EXIT_TIMEOUT_MSEC 100

/* Configuration struct for a theft run. */
struct theft_run_config {
    /* Property function under test.
     * The number refers to the number of generated arguments, and
     * should match the number of `theft_type_info` structs defined in
     * `.type_info` below. (The fields with different argument counts
     * are ignored.) */
    theft_propfun1 *prop1;
    theft_propfun2 *prop2;
    theft_propfun3 *prop3;
    theft_propfun4 *prop4;
    theft_propfun5 *prop5;
    theft_propfun6 *prop6;
    theft_propfun7 *prop7;

    /* Callbacks for allocating, freeing, printing, hashing,
     * and shrinking each property function argument. */
    const struct theft_type_info *type_info[THEFT_MAX_ARITY];

    /* -- All fields after this point are optional. -- */

    /* Property name, displayed in test runner output if non-NULL. */
    const char *name;

    /* Array of seeds to always run, and its length.
     * Can be used for regression tests. */
    size_t always_seed_count;      /* number of seeds */
    theft_seed *always_seeds;      /* seeds to always run */

    /* Number of trials to run. Defaults to THEFT_DEF_TRIALS. */
    size_t trials;

    /* Seed for the random number generator. */
    theft_seed seed;

    /* Bits to use for the bloom filter -- this field is no
     * longer used, and will be removed in a future release. */
    uint8_t bloom_bits;

    /* Fork before running the property test, in case generated
     * arguments can cause the code under test to crash. */
    struct {
        bool enable;
        size_t timeout;         /* in milliseconds (or 0, for none) */
        /* signal to send after timeout, defaults to SIGTERM */
        int signal;
        /* For workers sent a timeout signal, how long should
         * theft wait for them to actually exit (in msec).
         * Defaults to THEFT_DEF_EXIT_TIMEOUT_MSEC. */
        size_t exit_timeout;
    } fork;

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
        theft_hook_fork_post_cb *fork_post;
        theft_hook_trial_post_cb *trial_post;
        theft_hook_counterexample_cb *counterexample;
        theft_hook_shrink_pre_cb *shrink_pre;
        theft_hook_shrink_post_cb *shrink_post;
        theft_hook_shrink_trial_post_cb *shrink_trial_post;
        /* Environment pointer. This is completely opaque to theft
         * itself, but will be passed to all callbacks. */
        void *env;
    } hooks;
};

#endif
