#ifndef THEFT_RUN_INTERNAL_H
#define THEFT_RUN_INTERNAL_H

#include "theft_types_internal.h"

static bool
check_all_args(struct theft_propfun_info *info, bool *all_hashable);

static void
infer_arity(struct theft_propfun_info *info);

static enum all_gen_res_t
gen_all_args(struct theft *t, struct theft_propfun_info *info,
    theft_seed seed, void *args[THEFT_MAX_ARITY], void *env);

static enum theft_progress_callback_res
default_progress_cb(struct theft_trial_info *info, void *env);

#endif
