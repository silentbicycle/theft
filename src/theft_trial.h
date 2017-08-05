#ifndef THEFT_TRIAL_H
#define THEFT_TRIAL_H

bool
theft_trial_run(struct theft *t,
    struct theft_trial_info *trial_info,
    enum theft_hook_trial_post_res *tpres);

void
theft_trial_free_args(struct theft_run_info *run_info,
    void **args);

#endif
