#ifndef THEFT_TRIAL_INTERNAL_H
#define THEFT_TRIAL_INTERNAL_H

#include "theft_types_internal.h"
#include "theft_trial.h"

static enum theft_hook_trial_post_res
report_on_failure(struct theft *t,
    struct theft_hook_trial_post_info *hook_info,
    theft_hook_trial_post_cb *trial_post,
    void *trial_post_env);

theft_hook_trial_post_cb def_trial_post_cb;

#endif
