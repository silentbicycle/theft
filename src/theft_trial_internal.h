#ifndef THEFT_TRIAL_INTERNAL_H
#define THEFT_TRIAL_INTERNAL_H

#include "theft_types_internal.h"
#include "theft_trial.h"

static enum theft_progress_callback_res
report_on_failure(struct theft *t,
    struct theft_propfun_info *info,
    struct theft_trial_info *ti, theft_progress_cb *cb, void *env);

#endif
