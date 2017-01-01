#ifndef THEFT_INTERNAL_H
#define THEFT_INTERNAL_H

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
