#ifndef THEFT_RUN_INTERNAL_H
#define THEFT_RUN_INTERNAL_H

#include "theft_types_internal.h"
#include "theft_run.h"

static uint8_t
infer_arity(const struct theft_run_config *cfg);

enum run_step_res {
    RUN_STEP_OK,
    RUN_STEP_HALT,
    RUN_STEP_GEN_ERROR,
    RUN_STEP_TRIAL_ERROR,
};
static enum run_step_res
run_step(struct theft *t, size_t trial, theft_seed *seed);

static bool copy_propfun_for_arity(const struct theft_run_config *cfg,
    struct prop_info *prop);

static bool
check_all_args(uint8_t arity, const struct theft_run_config *cfg,
    bool *all_hashable);

enum all_gen_res {
    ALL_GEN_OK,                 /* all arguments generated okay */
    ALL_GEN_SKIP,               /* skip due to user constraints */
    ALL_GEN_DUP,                /* skip probably duplicated trial */
    ALL_GEN_ERROR,              /* memory error or other failure */
};

static bool init_arg_info(struct theft *t, struct trial_info *trial_info);

static enum all_gen_res
gen_all_args(struct theft *t);

static void free_print_trial_result_env(struct theft *t);

#endif
