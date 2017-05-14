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

    theft_hook_cb *cb = cfg->hook_cb
      ? cfg->hook_cb : default_hook_cb;

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
        .hook_cb = cb,
        .env = cfg->env,
    };
    memcpy(&run_info.type_info, cfg->type_info, sizeof(run_info.type_info));

    theft_seed seed = run_info.run_seed;
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

    struct theft_hook_info hook_info = {
        .prop_name = run_info.name,
        .total_trials = run_info.trial_count,
        .run_seed = run_info.run_seed,

        .type = THEFT_HOOK_TYPE_RUN_PRE,
        /* RUN_PRE has no other info in the union */
    };
    if (cb(&hook_info, run_info.env) != THEFT_HOOK_CONTINUE) {
        return THEFT_RUN_ERROR;
    }

    size_t limit = run_info.trial_count;

    for (size_t trial = 0; trial < limit; trial++) {
        void *args[THEFT_MAX_ARITY];
        enum run_step_res res = run_step(t, &run_info, &hook_info,
            trial, args, &seed);
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
            return THEFT_RUN_ERROR;
        }
    }

    hook_info = (struct theft_hook_info) {
        .prop_name = run_info.name,
        .total_trials = run_info.trial_count,
        .run_seed = run_info.run_seed,

        .type = THEFT_HOOK_TYPE_RUN_POST,
        .u.run_post.report = {
            .pass = run_info.pass,
            .fail = run_info.fail,
            .skip = run_info.skip,
            .dup = run_info.dup,
        },
    };

    if (cb(&hook_info, run_info.env) != THEFT_HOOK_CONTINUE) {
        return THEFT_RUN_ERROR;
    }

    free_any_autoshrink_wrappers(&run_info);

    if (run_info.fail > 0) {
        return THEFT_RUN_FAIL;
    } else {
        return THEFT_RUN_PASS;
    }
}

static enum run_step_res
run_step(struct theft *t, struct theft_run_info *run_info,
        struct theft_hook_info *hook_info,
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

    *hook_info = (struct theft_hook_info) {
        .prop_name = run_info->name,
        .total_trials = run_info->trial_count,
        .run_seed = run_info->run_seed,

        .type = THEFT_HOOK_TYPE_GEN_ARGS_PRE,
        .u.gen_args_pre = {
            .trial_id = trial,
            .trial_seed = trial_info.seed,
            .arity = run_info->arity
        },
    };
    enum theft_hook_res cres;
    cres = run_info->hook_cb(hook_info, run_info->env);

    switch (cres) {
    case THEFT_HOOK_CONTINUE:
        break;
    case THEFT_HOOK_HALT:
        return RUN_STEP_HALT;
    default:
        assert(false);
    case THEFT_HOOK_ERROR:
        return RUN_STEP_GEN_ERROR;
    }

    /* Set seed for this trial */
    theft_random_set_seed(t, trial_info.seed);

    enum all_gen_res_t gres = gen_all_args(t, run_info, args);
    *hook_info = (struct theft_hook_info) {
        .prop_name = run_info->name,
        .total_trials = run_info->trial_count,
        .run_seed = *seed,
        .type = THEFT_HOOK_TYPE_TRIAL_POST,
        .u.trial_post = {
            .trial_id = trial,
            .trial_seed = trial_info.seed,
            .arity = run_info->arity,
            .args = args,
        },
    };

    switch (gres) {
    case ALL_GEN_SKIP:
        /* skip generating these args */
        run_info->skip++;
        hook_info->u.trial_post.result = THEFT_TRIAL_SKIP;
        cres = run_info->hook_cb(hook_info, run_info->env);
        break;
    case ALL_GEN_DUP:
        /* skip these args -- probably already tried */
        run_info->dup++;
        hook_info->u.trial_post.result = THEFT_TRIAL_DUP;
        cres = run_info->hook_cb(hook_info, run_info->env);
        break;
    default:
    case ALL_GEN_ERROR:
        /* Error while generating args */
        hook_info->u.trial_post.result = THEFT_TRIAL_ERROR;
        cres = run_info->hook_cb(hook_info, run_info->env);
        return RUN_STEP_GEN_ERROR;
    case ALL_GEN_OK:
        *hook_info = (struct theft_hook_info) {
            .prop_name = run_info->name,
            .total_trials = run_info->trial_count,
            .run_seed = run_info->run_seed,
            .type = THEFT_HOOK_TYPE_TRIAL_PRE,
            .u.trial_pre = {
                .trial_id = trial,
                .trial_seed = trial_info.seed,
                .arity = run_info->arity,
                .args = args,
            },
        };
        cres = run_info->hook_cb(hook_info, run_info->env);
        if (cres == THEFT_HOOK_HALT) {
            return RUN_STEP_HALT;
        }
        if (!theft_trial_run(t, run_info, &trial_info, &cres)) {
            return RUN_STEP_TRIAL_ERROR;
        }
    }

    /* Update seed for next trial */
    *seed = theft_random(t);
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

static enum theft_hook_res
default_hook_cb(const struct theft_hook_info *info, void *env) {
    (void)info;
    (void)env;
    return THEFT_HOOK_CONTINUE;
}

static bool wrap_any_autoshrinks(struct theft *t,
        struct theft_run_info *info) {
    for (uint8_t i = 0; i < info->arity; i++) {
        struct theft_type_info *ti = info->type_info[i];
        if (ti->autoshrink_config.enable) {
            struct theft_type_info *new_ti = calloc(1, sizeof(*new_ti));
            if (new_ti == NULL) {
                return false;
            }
            if (!theft_autoshrink_wrap(t, ti, new_ti)) {
                return false;   /* alloc fail */
            }

            info->type_info[i] = new_ti;
        }
    }

    return true;
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
