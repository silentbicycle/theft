#ifndef THEFT_TRIAL_INTERNAL_H
#define THEFT_TRIAL_INTERNAL_H

#include "theft_types_internal.h"
#include "theft_trial.h"

static enum theft_hook_res
report_on_failure(struct theft *t,
    struct theft_run_info *run_info,
    struct theft_trial_info *trial_info,
    struct theft_hook_info *hook_info);

#endif
