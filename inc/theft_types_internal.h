#ifndef THEFT_TYPES_INTERNAL_H
#define THEFT_TYPES_INTERNAL_H

#define THEFT_MAX_TACTICS ((uint32_t)-1)
#define DEFAULT_THEFT_SEED 0xa600d16b175eedL

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

static enum theft_trial_res
call_fun(struct theft_propfun_info *info, void **args);

static bool
run_trial(struct theft *t, struct theft_propfun_info *info,
    void **args, theft_progress_cb *cb, void *env,
    struct theft_trial_report *r, struct theft_trial_info *ti,
    enum theft_progress_callback_res *cres);

static void
mark_called(struct theft *t, struct theft_propfun_info *info,
    void **args, void *env);

static bool
check_called(struct theft *t, struct theft_propfun_info *info,
    void **args, void *env);

static enum theft_progress_callback_res
report_on_failure(struct theft *t,
    struct theft_propfun_info *info,
    struct theft_trial_info *ti, theft_progress_cb *cb, void *env);

static enum all_gen_res_t
gen_all_args(struct theft *t, struct theft_propfun_info *info,
    theft_seed seed, void *args[THEFT_MAX_ARITY], void *env);

static void
free_args(struct theft_propfun_info *info,
    void **args, void *env);

static enum theft_run_res
theft_run_internal(struct theft *t,
    struct theft_propfun_info *info,
    int trials, theft_progress_cb *cb, void *env,
    struct theft_trial_report *r);

static bool
attempt_to_shrink(struct theft *t, struct theft_propfun_info *info,
    void *args[], void *env);

static enum shrink_res
attempt_to_shrink_arg(struct theft *t, struct theft_propfun_info *info,
    void *args[], void *env, int ai);

#endif
