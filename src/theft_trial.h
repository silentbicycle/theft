#ifndef THEFT_TRIAL_H
#define THEFT_TRIAL_H

bool
theft_trial_run(struct theft *t, struct theft_propfun_info *info,
    void **args, theft_progress_cb *cb, void *env,
    struct theft_trial_report *r, struct theft_trial_info *ti,
    enum theft_progress_callback_res *cres);

void
theft_trial_free_args(struct theft_propfun_info *info,
    void **args, void *env);

#endif
