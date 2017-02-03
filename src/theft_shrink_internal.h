#ifndef THEFT_SHRINK_INTERNAL_H
#define THEFT_SHRINK_INTERNAL_H

#include "theft_types_internal.h"

static enum shrink_res
attempt_to_shrink_arg(struct theft *t,
    struct theft_run_info *run_info,
    struct theft_trial_info *trial_info, uint8_t arg_i);

static enum theft_progress_callback_res
pre_shrink_progress(struct theft_run_info *run_info,
    struct theft_trial_info *trial_info,
    uint8_t arg_index, void *arg, uint32_t tactic);

static enum theft_progress_callback_res
post_shrink_progress(struct theft_run_info *run_info,
    struct theft_trial_info *trial_info,
    uint8_t arg_index, void *arg, uint32_t tactic, bool done);

static enum theft_progress_callback_res
post_shrink_trial_progress(struct theft_run_info *run_info,
    struct theft_trial_info *trial_info,
    uint8_t arg_index, void **args, uint32_t last_tactic,
    enum theft_trial_res result);

#endif
