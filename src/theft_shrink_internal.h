#ifndef THEFT_SHRINK_INTERNAL_H
#define THEFT_SHRINK_INTERNAL_H

#include "theft_types_internal.h"

enum shrink_res {
    SHRINK_OK,                  /* simplified argument further */
    SHRINK_DEAD_END,            /* at local minima */
    SHRINK_ERROR,               /* hard error during shrinking */
    SHRINK_HALT,                /* don't shrink any further */
};

static enum shrink_res
attempt_to_shrink_arg(struct theft *t, uint8_t arg_i);

static enum theft_hook_shrink_pre_res
shrink_pre_hook(struct theft *t,
    uint8_t arg_index, void *arg, uint32_t tactic);

static enum theft_hook_shrink_post_res
shrink_post_hook(struct theft *t,
    uint8_t arg_index, void *arg, uint32_t tactic,
    enum theft_shrink_res sres);

static enum theft_hook_shrink_trial_post_res
shrink_trial_post_hook(struct theft *t,
    uint8_t arg_index, void **args, uint32_t last_tactic,
    enum theft_trial_res result);

#endif
