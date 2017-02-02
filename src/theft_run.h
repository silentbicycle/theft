#ifndef THEFT_RUN_H
#define THEFT_RUN_H

/* Actually run the trials, with all arguments made explicit. */
enum theft_run_res
theft_run_trials(struct theft *t,
    struct theft_propfun_info *info,
    int trials, theft_progress_cb *cb, void *env,
    struct theft_trial_report *r);

#endif
