#ifndef THEFT_RUN_INTERNAL_H
#define THEFT_RUN_INTERNAL_H

#include "theft_types_internal.h"

static uint8_t
infer_arity(const struct theft_run_config *cfg);

enum run_step_res {
    RUN_STEP_OK,
    RUN_STEP_HALT,
    RUN_STEP_GEN_ERROR,
    RUN_STEP_TRIAL_ERROR,
};
static enum run_step_res
run_step(struct theft *t, struct theft_run_info *run_info,
    size_t trial, void **args, theft_seed *seed);

static bool
check_all_args(uint8_t arity, const struct theft_run_config *cfg,
    bool *all_hashable);

static enum all_gen_res_t
gen_all_args(struct theft *t, struct theft_run_info *info,
    void *args[THEFT_MAX_ARITY]);

static bool wrap_any_autoshrinks(struct theft *t,
    struct theft_run_info *info);

static void free_any_autoshrink_wrappers(struct theft_run_info *info);

static void free_print_trial_result_env(struct theft_run_info *info);

#endif
