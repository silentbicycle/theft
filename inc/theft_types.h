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

struct theft_config {
    /* The number of bits to use for the bloom filter, which
     * detects combinations of arguments that have already
     * been tested. If 0, a default size will be chosen
     * based on the trial count. (This will only be used if
     * all property types have hash callbacks defined.) */
    uint8_t bloom_bits;
};

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

    /* TODO comment */
    bool autoshrink;
    
    /* Optional environment, passed to the callbacks above. */
    void *env;
};

/* Type tags for the info given to the hook callback. */
enum theft_hook_type {
    /* Before the start of a run (group of trials). */
    THEFT_HOOK_TYPE_RUN_PRE,

    /* After the whole run has completed, with overall results. */
    THEFT_HOOK_TYPE_RUN_POST,

    /* Before generating the argument(s) for a trial. */
    THEFT_HOOK_TYPE_GEN_ARGS_PRE,

    /* Before running the trial, with the generated argument(s). */
    THEFT_HOOK_TYPE_TRIAL_PRE,

    /* After running the trial, with the arguments and result. */
    THEFT_HOOK_TYPE_TRIAL_POST,

    /* Before attempting to shrink with the next tactic. */
    THEFT_HOOK_TYPE_SHRINK_PRE,

    /* After attempting to shrink, with the new instance. */
    THEFT_HOOK_TYPE_SHRINK_POST,

    /* After re-running the trial with shrunken argument(s), with its result. */
    THEFT_HOOK_TYPE_SHRINK_TRIAL_POST,
};

/* Overall trial pass/fail/skip/duplicate counts after a run. */
struct theft_run_report {
    size_t pass;
    size_t fail;
    size_t skip;
    size_t dup;
};

/* structs contained within theft_hook_info's tagged union, below. */
struct theft_hook_run_post {
    struct theft_run_report report;
};
struct theft_hook_gen_args_pre {
    size_t trial_id;
    theft_seed trial_seed;
    uint8_t arity;
};
struct theft_hook_trial_pre {
    size_t trial_id;
    theft_seed trial_seed;
    uint8_t arity;
    void **args;
};
struct theft_hook_trial_post {
    size_t trial_id;
    theft_seed trial_seed;
    uint8_t arity;
    void **args;
    enum theft_trial_res result;
};
struct theft_hook_shrink_pre {
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
struct theft_hook_shrink_post {
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
struct theft_hook_shrink_trial_post {
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

/* Info given to the hook callback.
 * The union is tagged by the TYPE field. */
struct theft_hook_info {
    /* Fields that are always set. */
    const char *prop_name;
    size_t total_trials;
    theft_seed run_seed;

    /* Tagged union. */
    enum theft_hook_type type;
    union {
        // run_pre has no other fields
        struct theft_hook_run_post run_post;
        struct theft_hook_gen_args_pre gen_args_pre;
        struct theft_hook_trial_pre trial_pre;
        struct theft_hook_trial_post trial_post;
        struct theft_hook_shrink_pre shrink_pre;
        struct theft_hook_shrink_post shrink_post;
        struct theft_hook_shrink_trial_post shrink_trial_post;
    } u;
};

/* Whether to keep running trials after N failures/skips/etc. */
enum theft_hook_res {
    THEFT_HOOK_ERROR,       /* error, cancel entire run */
    THEFT_HOOK_CONTINUE,    /* continue current step */

    /* Halt the current step, but continue. This could be used to halt
     * shrinking when it hasn't made any progress for a while, or to
     * halt a run when there have been too many failed trials.
     *
     * Only valid in GEN_ARGS_PRE, TRIAL_PRE and SHRINK_PRE. */
    THEFT_HOOK_HALT,

    /* Repeat the current step. This could be used to re-run the
     * property function with the same argument instances, but with more
     * logging, breakpoints added, etc.
     *
     * Only valid in TRIAL_POST, SHRINK_TRIAL_POST. */
    THEFT_HOOK_REPEAT,

    /* Same as REPEAT, but only once. This is so the theft caller
     * doesn't need to track repeated calls to avoid infinite loops. */
    THEFT_HOOK_REPEAT_ONCE,
};

/* Handle test results.
 * Can be used to halt after too many failures, print '.' after
 * every N trials, etc. */
typedef enum theft_hook_res
theft_hook_cb(const struct theft_hook_info *info, void *env);

/* Result from a trial run. */
enum theft_run_res {
    THEFT_RUN_PASS = 0,             /* no failures */
    THEFT_RUN_FAIL = 1,             /* 1 or more failures */
    THEFT_RUN_ERROR = 2,            /* an error occurred */
    THEFT_RUN_ERROR_BAD_ARGS = -1,  /* API misuse */
    /* Missing required callback for 1 or more types */
    THEFT_RUN_ERROR_MISSING_CALLBACK = -2,
};

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

    /* Hook callback, called in several contexts to report on progress,
     * halt shrinking early, repeat trials with different logging, etc.
     * See struct theft_hook_info above for details. */
    theft_hook_cb *hook;

    /* Environment pointer. This is completely opaque to theft itself,
     * but will be passed along to all callbacks. */
    void *env;

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
