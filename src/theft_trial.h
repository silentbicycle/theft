#ifndef THEFT_TRIAL_H
#define THEFT_TRIAL_H

bool
theft_trial_run(struct theft *t, struct theft_run_info *run_info,
    struct theft_trial_info *trial_info,
    enum theft_progress_callback_res *cres);

void
theft_trial_free_args(struct theft_run_info *info,
    void **args);

#endif
