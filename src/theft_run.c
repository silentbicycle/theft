#include "theft_run_internal.h"

#include "theft_bloom.h"
#include "theft_call.h"
#include "theft_trial.h"
#include "theft_random.h"
#include "theft_autoshrink.h"

#include <string.h>
#include <assert.h>

/* Actually run the trials, with all arguments made explicit. */
enum theft_run_res
theft_run_trials(struct theft *t, const struct theft_run_config *cfg) {
    const uint8_t arity = infer_arity(cfg);
    if (arity == 0) {
        return THEFT_RUN_ERROR_BAD_ARGS;
    }

    bool all_hashable = false;
    if (!check_all_args(arity, cfg, &all_hashable)) {
        return THEFT_RUN_ERROR_MISSING_CALLBACK;
    }

    struct theft_run_info run_info = {
        .name = cfg->name,
        .fun = cfg->fun,
        .trial_count = cfg->trials == 0 ? THEFT_DEF_TRIALS : cfg->trials,
        .run_seed = cfg->seed ? cfg->seed : DEFAULT_THEFT_SEED,
        .arity = arity,
        /* type_info is memcpy'd below */
        .always_seed_count = (cfg->always_seeds == NULL
            ? 0 : cfg->always_seed_count),
        .always_seeds = cfg->always_seeds,
        .hooks = {
            .run_pre = (cfg->hooks.run_pre != NULL
                ? cfg->hooks.run_pre
                : theft_hook_run_pre_print_info),
            .run_post = (cfg->hooks.run_post != NULL
                ? cfg->hooks.run_post
                : theft_hook_run_post_print_info),
            .gen_args_pre = cfg->hooks.gen_args_pre,
            .trial_pre = cfg->hooks.trial_pre,
            .trial_post = (cfg->hooks.trial_post != NULL
                ? cfg->hooks.trial_post
                : theft_hook_trial_post_print_result),

            .counterexample = (cfg->hooks.counterexample != NULL
                ? cfg->hooks.counterexample
                : theft_print_counterexample),
            .shrink_pre = cfg->hooks.shrink_pre,
            .shrink_post = cfg->hooks.shrink_post,
            .shrink_trial_post = cfg->hooks.shrink_trial_post,
            .env = cfg->hooks.env,
        },
    };
    memcpy(&run_info.type_info, cfg->type_info, sizeof(run_info.type_info));

    theft_seed seed = run_info.run_seed;
    LOG(3, "%s: SETTING RUN SEED TO 0x%016" PRIx64 "\n", __func__, seed);
    theft_random_set_seed(t, seed);
    if (!wrap_any_autoshrinks(t, &run_info)) {
        return THEFT_RUN_ERROR;
    }

    /* If all arguments are hashable, then attempt to use
     * a bloom filter to avoid redundant checking. */
    if (all_hashable) {
        if (t->requested_bloom_bits == 0) {
            t->requested_bloom_bits = theft_bloom_recommendation(run_info.trial_count);
        }
        t->bloom = theft_bloom_init(t->requested_bloom_bits);
    }

    if (run_info.hooks.trial_post == theft_hook_trial_post_print_result) {
        run_info.print_trial_result_env = calloc(1,
            sizeof(*run_info.print_trial_result_env));
        if (run_info.print_trial_result_env == NULL) {
            return THEFT_RUN_ERROR;
        }
    }

    if (run_info.hooks.run_pre != NULL) {
        struct theft_hook_run_pre_info hook_info = {
            .prop_name = run_info.name,
            .total_trials = run_info.trial_count,
            .run_seed = run_info.run_seed,
        };
        enum theft_hook_run_pre_res res = run_info.hooks.run_pre(&hook_info, run_info.hooks.env);
        if (res != THEFT_HOOK_RUN_PRE_CONTINUE) {
            free_any_autoshrink_wrappers(&run_info);
            free_print_trial_result_env(&run_info);
            return THEFT_RUN_ERROR;
        }
    }

    size_t limit = run_info.trial_count;

    for (size_t trial = 0; trial < limit; trial++) {
        void *args[THEFT_MAX_ARITY];
        enum run_step_res res = run_step(t, &run_info,
            trial, args, &seed);
        LOG(3, "  -- trial %zd/%zd, new seed 0x%016" PRIx64 "\n",
            trial, limit, seed);
        theft_trial_free_args(&run_info, args);

        switch (res) {
        case RUN_STEP_OK:
            continue;
        case RUN_STEP_HALT:
            limit = trial;
            break;
        default:
        case RUN_STEP_GEN_ERROR:
        case RUN_STEP_TRIAL_ERROR:
            free_any_autoshrink_wrappers(&run_info);
            free_print_trial_result_env(&run_info);
            return THEFT_RUN_ERROR;
        }
    }

    theft_hook_run_post_cb *run_post = run_info.hooks.run_post;
    if (run_post != NULL) {
        struct theft_hook_run_post_info hook_info = {
            .prop_name = run_info.name,
            .total_trials = run_info.trial_count,
            .run_seed = run_info.run_seed,
            .report = {
                .pass = run_info.pass,
                .fail = run_info.fail,
                .skip = run_info.skip,
                .dup = run_info.dup,
            },
        };

        enum theft_hook_run_post_res res = run_post(&hook_info, run_info.hooks.env);
        if (res != THEFT_HOOK_RUN_POST_CONTINUE) {
            free_any_autoshrink_wrappers(&run_info);
            free_print_trial_result_env(&run_info);
            return THEFT_RUN_ERROR;
        }
    }

    free_any_autoshrink_wrappers(&run_info);
    free_print_trial_result_env(&run_info);

    if (run_info.fail > 0) {
        return THEFT_RUN_FAIL;
    } else if (run_info.pass > 0) {
        return THEFT_RUN_PASS;
    } else {
        return THEFT_RUN_SKIP;
    }
}

static enum run_step_res
run_step(struct theft *t, struct theft_run_info *run_info,
        size_t trial, void **args, theft_seed *seed) {
    memset(args, 0x00, THEFT_MAX_ARITY * sizeof(args[0]));

    /* If any seeds to always run were specified, use those before
     * reverting to the specified starting seed. */
    const size_t always_seeds = run_info->always_seed_count;
    if (trial < always_seeds) {
        *seed = run_info->always_seeds[trial];
    } else if ((always_seeds > 0) && (trial == always_seeds)) {
        *seed = run_info->run_seed;
    }

    struct theft_trial_info trial_info = {
        .trial = trial,
        .seed = *seed,
        .arity = run_info->arity,
        .args = args
    };

    theft_hook_gen_args_pre_cb *gen_args_pre = run_info->hooks.gen_args_pre;
    if (gen_args_pre != NULL) {
        struct theft_hook_gen_args_pre_info hook_info = {
            .prop_name = run_info->name,
            .total_trials = run_info->trial_count,
            .failures = run_info->fail,
            .run_seed = run_info->run_seed,
            .trial_id = trial,
            .trial_seed = trial_info.seed,
            .arity = run_info->arity
        };
        enum theft_hook_gen_args_pre_res res = gen_args_pre(&hook_info,
            run_info->hooks.env);

        switch (res) {
        case THEFT_HOOK_GEN_ARGS_PRE_CONTINUE:
            break;
        case THEFT_HOOK_GEN_ARGS_PRE_HALT:
            return RUN_STEP_HALT;
        default:
            assert(false);
        case THEFT_HOOK_GEN_ARGS_PRE_ERROR:
            return RUN_STEP_GEN_ERROR;
        }
    }

    /* Set seed for this trial */
    LOG(3, "%s: SETTING TRIAL SEED TO 0x%016" PRIx64 "\n", __func__, trial_info.seed);
    theft_random_set_seed(t, trial_info.seed);

    enum all_gen_res_t gres = gen_all_args(t, run_info, args);
    theft_hook_trial_post_cb *post_cb = run_info->hooks.trial_post;
    void *hook_env = (run_info->hooks.trial_post == theft_hook_trial_post_print_result
        ? run_info->print_trial_result_env
        : run_info->hooks.env);

    struct theft_hook_trial_post_info hook_info = {
        .t = t,
        .prop_name = run_info->name,
        .total_trials = run_info->trial_count,
        .failures = run_info->fail,
        .run_seed = *seed,
        .trial_id = trial,
        .trial_seed = trial_info.seed,
        .arity = run_info->arity,
        .args = args,
    };

    enum theft_hook_trial_post_res pres;

    switch (gres) {
    case ALL_GEN_SKIP:
        /* skip generating these args */
        LOG(3, "gen -- skip\n");
        run_info->skip++;
        hook_info.result = THEFT_TRIAL_SKIP;
        pres = post_cb(&hook_info, hook_env);
        break;
    case ALL_GEN_DUP:
        /* skip these args -- probably already tried */
        LOG(3, "gen -- dup\n");
        run_info->dup++;
        hook_info.result = THEFT_TRIAL_DUP;
        pres = post_cb(&hook_info, hook_env);
        break;
    default:
    case ALL_GEN_ERROR:
        /* Error while generating args */
        LOG(1, "gen -- error\n");
        hook_info.result = THEFT_TRIAL_ERROR;
        pres = post_cb(&hook_info, hook_env);
        return RUN_STEP_GEN_ERROR;
    case ALL_GEN_OK:
        LOG(4, "gen -- ok\n");
        if (run_info->hooks.trial_pre != NULL) {
            struct theft_hook_trial_pre_info info = {
                .prop_name = run_info->name,
                .total_trials = run_info->trial_count,
                .failures = run_info->fail,
                .run_seed = run_info->run_seed,
                .trial_id = trial,
                .trial_seed = trial_info.seed,
                .arity = run_info->arity,
                .args = args,
            };
            enum theft_hook_trial_pre_res tpres;
            tpres = run_info->hooks.trial_pre(&info, hook_env);
            if (tpres == THEFT_HOOK_TRIAL_PRE_HALT) {
                return RUN_STEP_HALT;
            } else if (tpres == THEFT_HOOK_TRIAL_PRE_ERROR) {
                return RUN_STEP_TRIAL_ERROR;
            }
        }

        if (!theft_trial_run(t, run_info, &trial_info, &pres)) {
            return RUN_STEP_TRIAL_ERROR;
        }
    }

    if (pres == THEFT_HOOK_TRIAL_POST_ERROR) {
        return RUN_STEP_TRIAL_ERROR;
    }

    /* Update seed for next trial */
    *seed = theft_random(t);
    LOG(3, "end of trial, new seed is 0x%016" PRIx64 "\n", *seed);
    return RUN_STEP_OK;
}

static uint8_t
infer_arity(const struct theft_run_config *cfg) {
    for (uint8_t i = 0; i < THEFT_MAX_ARITY; i++) {
        if (cfg->type_info[i] == NULL) {
            return i;
        }
    }
    return 0;
}

/* Check if all argument info structs have all required callbacks. */
static bool
check_all_args(uint8_t arity, const struct theft_run_config *cfg,
        bool *all_hashable) {
    bool ah = true;
    for (int i = 0; i < arity; i++) {
        const struct theft_type_info *ti = cfg->type_info[i];
        if (ti->alloc == NULL) { return false; }
        if (ti->hash == NULL && !ti->autoshrink_config.enable) {
            ah = false;
        }
    }
    *all_hashable = ah;
    return true;
}

/* Attempt to instantiate arguments, starting with the current seed. */
static enum all_gen_res_t
gen_all_args(struct theft *t, struct theft_run_info *run_info,
        void *args[THEFT_MAX_ARITY]) {
    for (int i = 0; i < run_info->arity; i++) {
        struct theft_type_info *ti = run_info->type_info[i];
        void *p = NULL;
        enum theft_alloc_res res = ti->alloc(t, ti->env, &p);
        if (res == THEFT_ALLOC_SKIP || res == THEFT_ALLOC_ERROR) {
            for (int j = 0; j < i; j++) {
                ti->free(args[j], ti->env);
            }
            if (res == THEFT_ALLOC_SKIP) {
                return ALL_GEN_SKIP;
            } else {
                return ALL_GEN_ERROR;
            }
        } else {
            args[i] = p;
        }
    }

    /* check bloom filter */
    if (t->bloom && theft_call_check_called(t, run_info, args)) {
        return ALL_GEN_DUP;
    }

    return ALL_GEN_OK;
}

static bool wrap_any_autoshrinks(struct theft *t,
        struct theft_run_info *info) {
    uint8_t wrapped = 0;
    for (uint8_t i = 0; i < info->arity; i++) {
        struct theft_type_info *ti = info->type_info[i];
        if (ti->autoshrink_config.enable) {
            struct theft_type_info *new_ti = calloc(1, sizeof(*new_ti));
            if (new_ti == NULL) {
                goto cleanup;
            }
            if (!theft_autoshrink_wrap(t, ti, new_ti)) {
                goto cleanup;
            }
            wrapped++;

            info->type_info[i] = new_ti;
        }
    }

    return true;
cleanup:
    for (uint8_t i = 0; i < wrapped; i++) {
        struct theft_type_info *ti = info->type_info[i];
        if (ti->autoshrink_config.enable) {
            struct theft_autoshrink_env *env =
              (struct theft_autoshrink_env *)ti->env;
            assert(env->tag == AUTOSHRINK_ENV_TAG);
            free(env);
            free(ti);
        }
    }
    return false;
}

static void free_any_autoshrink_wrappers(struct theft_run_info *info) {
    for (uint8_t i = 0; i < info->arity; i++) {
        struct theft_type_info *ti = info->type_info[i];
        if (ti->autoshrink_config.enable) {
            struct theft_autoshrink_env *env =
              (struct theft_autoshrink_env *)ti->env;
            assert(env->tag == AUTOSHRINK_ENV_TAG);
            free(env);
            free(ti);
        }
    }
}

static void free_print_trial_result_env(struct theft_run_info *run_info) {
    if (run_info->hooks.trial_post == theft_hook_trial_post_print_result
        && run_info->print_trial_result_env != NULL) {
        free(run_info->print_trial_result_env);
        run_info->print_trial_result_env = NULL;
    }
}
