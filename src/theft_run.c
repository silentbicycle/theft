#include "theft_run_internal.h"

#include "theft_bloom.h"
#include "theft_rng.h"
#include "theft_call.h"
#include "theft_trial.h"
#include "theft_random.h"
#include "theft_autoshrink.h"

#include <string.h>
#include <assert.h>

#define LOG_RUN 0

enum theft_run_init_res
theft_run_init(const struct theft_run_config *cfg, struct theft **output) {
    enum theft_run_init_res res = THEFT_RUN_INIT_OK;
    struct theft *t = malloc(sizeof(*t));
    if (t == NULL) {
        return THEFT_RUN_INIT_ERROR_MEMORY;
    }
    memset(t, 0, sizeof(*t));

    t->out = stdout;
    t->prng.rng = theft_rng_init(DEFAULT_THEFT_SEED);
    if (t->prng.rng == NULL) {
        free(t);
        return THEFT_RUN_INIT_ERROR_MEMORY;
    }

    const uint8_t arity = infer_arity(cfg);
    if (arity == 0) {
        res = THEFT_RUN_INIT_ERROR_BAD_ARGS;
        goto cleanup;
    }

    bool all_hashable = false;
    if (!check_all_args(arity, cfg, &all_hashable)) {
        res = THEFT_RUN_INIT_ERROR_BAD_ARGS;
        goto cleanup;
    }

    struct seed_info seeds = {
        .run_seed = cfg->seed ? cfg->seed : DEFAULT_THEFT_SEED,
        .always_seed_count = (cfg->always_seeds == NULL
            ? 0 : cfg->always_seed_count),
        .always_seeds = cfg->always_seeds,
    };
    memcpy(&t->seeds, &seeds, sizeof(seeds));

    struct fork_info fork = {
        .enable = cfg->fork.enable,
        .timeout = cfg->fork.timeout,
        .signal = cfg->fork.signal,
        .exit_timeout = cfg->fork.exit_timeout,
    };
    memcpy(&t->fork, &fork, sizeof(fork));

    struct prop_info prop = {
        .name = cfg->name,
        .arity = arity,
        .trial_count = cfg->trials == 0 ? THEFT_DEF_TRIALS : cfg->trials,
        /* .type_info is memcpy'd below */
    };
    if (!copy_propfun_for_arity(cfg, &prop)) {
        res = THEFT_RUN_INIT_ERROR_BAD_ARGS;
        goto cleanup;
    }
    memcpy(&prop.type_info, cfg->type_info, sizeof(prop.type_info));
    memcpy(&t->prop, &prop, sizeof(prop));

    struct hook_info hooks = {
        .run_pre = (cfg->hooks.run_pre != NULL
            ? cfg->hooks.run_pre
            : theft_hook_run_pre_print_info),
        .run_post = (cfg->hooks.run_post != NULL
            ? cfg->hooks.run_post
            : theft_hook_run_post_print_info),
        .gen_args_pre = cfg->hooks.gen_args_pre,
        .trial_pre = cfg->hooks.trial_pre,
        .fork_post = cfg->hooks.fork_post,
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
    };
    memcpy(&t->hooks, &hooks, sizeof(hooks));

    LOG(3 - LOG_RUN,
        "%s: SETTING RUN SEED TO 0x%016" PRIx64 "\n",
        __func__, t->seeds.run_seed);
    theft_random_set_seed(t, t->seeds.run_seed);

    /* If all arguments are hashable, then attempt to use
     * a bloom filter to avoid redundant checking. */
    if (all_hashable) {
        t->bloom = theft_bloom_init(NULL);
    }

    /* If using the default trial_post callback, allocate its
     * environment, with info relating to printing progress. */
    if (t->hooks.trial_post == theft_hook_trial_post_print_result) {
        t->print_trial_result_env = calloc(1,
            sizeof(*t->print_trial_result_env));
        if (t->print_trial_result_env == NULL) {
            return THEFT_RUN_INIT_ERROR_MEMORY;
        }
        t->print_trial_result_env->tag = THEFT_PRINT_TRIAL_RESULT_ENV_TAG;
    }

    *output = t;
    return res;

cleanup:
    theft_rng_free(t->prng.rng);
    free(t);
    return res;
}

void theft_run_free(struct theft *t) {
    if (t->bloom) {
        theft_bloom_free(t->bloom);
        t->bloom = NULL;
    }
    theft_rng_free(t->prng.rng);

    if (t->print_trial_result_env != NULL) {
        free(t->print_trial_result_env);
    }

    free(t);
}

/* Actually run the trials, with all arguments made explicit. */
enum theft_run_res
theft_run_trials(struct theft *t) {
    if (t->hooks.run_pre != NULL) {
        struct theft_hook_run_pre_info hook_info = {
            .prop_name = t->prop.name,
            .total_trials = t->prop.trial_count,
            .run_seed = t->seeds.run_seed,
        };
        enum theft_hook_run_pre_res res = t->hooks.run_pre(&hook_info, t->hooks.env);
        if (res != THEFT_HOOK_RUN_PRE_CONTINUE) {
            goto cleanup;
        }
    }

    size_t limit = t->prop.trial_count;
    theft_seed seed = t->seeds.run_seed;

    for (size_t trial = 0; trial < limit; trial++) {
        enum run_step_res res = run_step(t, trial, &seed);
        memset(&t->trial, 0x00, sizeof(t->trial));

        LOG(3 - LOG_RUN,
            "  -- trial %zd/%zd, new seed 0x%016" PRIx64 "\n",
            trial, limit, seed);

        switch (res) {
        case RUN_STEP_OK:
            continue;
        case RUN_STEP_HALT:
            limit = trial;
            break;
        default:
        case RUN_STEP_GEN_ERROR:
        case RUN_STEP_TRIAL_ERROR:
            goto cleanup;
        }
    }

    theft_hook_run_post_cb *run_post = t->hooks.run_post;
    if (run_post != NULL) {
        struct theft_hook_run_post_info hook_info = {
            .prop_name = t->prop.name,
            .total_trials = t->prop.trial_count,
            .run_seed = t->seeds.run_seed,
            .report = {
                .pass = t->counters.pass,
                .fail = t->counters.fail,
                .skip = t->counters.skip,
                .dup = t->counters.dup,
            },
        };

        enum theft_hook_run_post_res res = run_post(&hook_info, t->hooks.env);
        if (res != THEFT_HOOK_RUN_POST_CONTINUE) {
            goto cleanup;
        }
    }

    free_print_trial_result_env(t);

    if (t->counters.fail > 0) {
        return THEFT_RUN_FAIL;
    } else if (t->counters.pass > 0) {
        return THEFT_RUN_PASS;
    } else {
        return THEFT_RUN_SKIP;
    }

cleanup:
    free_print_trial_result_env(t);
    return THEFT_RUN_ERROR;
}

static enum run_step_res
run_step(struct theft *t, size_t trial, theft_seed *seed) {
    /* If any seeds to always run were specified, use those before
     * reverting to the specified starting seed. */
    const size_t always_seeds = t->seeds.always_seed_count;
    if (trial < always_seeds) {
        *seed = t->seeds.always_seeds[trial];
    } else if ((always_seeds > 0) && (trial == always_seeds)) {
        *seed = t->seeds.run_seed;
    }

    struct trial_info trial_info = {
        .trial = trial,
        .seed = *seed,
    };
    if (!init_arg_info(t, &trial_info)) { return RUN_STEP_GEN_ERROR; }

    memcpy(&t->trial, &trial_info, sizeof(trial_info));

    theft_hook_gen_args_pre_cb *gen_args_pre = t->hooks.gen_args_pre;
    if (gen_args_pre != NULL) {
        struct theft_hook_gen_args_pre_info hook_info = {
            .prop_name = t->prop.name,
            .total_trials = t->prop.trial_count,
            .failures = t->counters.fail,
            .run_seed = t->seeds.run_seed,
            .trial_id = t->trial.trial,
            .trial_seed = t->trial.seed,
            .arity = t->prop.arity
        };
        enum theft_hook_gen_args_pre_res res = gen_args_pre(&hook_info,
            t->hooks.env);

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
    LOG(3 - LOG_RUN,
        "%s: SETTING TRIAL SEED TO 0x%016" PRIx64 "\n", __func__, trial_info.seed);
    theft_random_set_seed(t, trial_info.seed);

    enum run_step_res res = RUN_STEP_OK;
    enum all_gen_res gres = gen_all_args(t);
    /* anything after this point needs to free all args */

    theft_hook_trial_post_cb *post_cb = t->hooks.trial_post;
    void *hook_env = (t->hooks.trial_post == theft_hook_trial_post_print_result
        ? t->print_trial_result_env
        : t->hooks.env);

    void *args[THEFT_MAX_ARITY];
    for (size_t i = 0; i < t->prop.arity; i++) {
        args[i] = t->trial.args[i].instance;
    }

    struct theft_hook_trial_post_info hook_info = {
        .t = t,
        .prop_name = t->prop.name,
        .total_trials = t->prop.trial_count,
        .failures = t->counters.fail,
        .run_seed = *seed,
        .trial_id = trial,
        .trial_seed = trial_info.seed,
        .arity = t->prop.arity,
        .args = args,
    };

    enum theft_hook_trial_post_res pres;

    switch (gres) {
    case ALL_GEN_SKIP:          /* skip generating these args */
        LOG(3 - LOG_RUN, "gen -- skip\n");
        t->counters.skip++;
        hook_info.result = THEFT_TRIAL_SKIP;
        pres = post_cb(&hook_info, hook_env);
        break;
    case ALL_GEN_DUP:           /* skip these args -- probably already tried */
        LOG(3 - LOG_RUN, "gen -- dup\n");
        t->counters.dup++;
        hook_info.result = THEFT_TRIAL_DUP;
        pres = post_cb(&hook_info, hook_env);
        break;
    default:
    case ALL_GEN_ERROR:         /* error while generating args */
        LOG(1 - LOG_RUN, "gen -- error\n");
        hook_info.result = THEFT_TRIAL_ERROR;
        pres = post_cb(&hook_info, hook_env);
        res = RUN_STEP_GEN_ERROR;
        goto cleanup;
    case ALL_GEN_OK:
        LOG(4 - LOG_RUN, "gen -- ok\n");
        if (t->hooks.trial_pre != NULL) {
            struct theft_hook_trial_pre_info info = {
                .prop_name = t->prop.name,
                .total_trials = t->prop.trial_count,
                .failures = t->counters.fail,
                .run_seed = t->seeds.run_seed,
                .trial_id = trial,
                .trial_seed = trial_info.seed,
                .arity = t->prop.arity,
            };

            enum theft_hook_trial_pre_res tpres;
            tpres = t->hooks.trial_pre(&info, t->hooks.env);
            if (tpres == THEFT_HOOK_TRIAL_PRE_HALT) {
                res = RUN_STEP_HALT;
                goto cleanup;
            } else if (tpres == THEFT_HOOK_TRIAL_PRE_ERROR) {
                res = RUN_STEP_TRIAL_ERROR;
                goto cleanup;
            }
        }

        if (!theft_trial_run(t, &pres)) {
            res = RUN_STEP_TRIAL_ERROR;
            goto cleanup;
        }
    }

    if (pres == THEFT_HOOK_TRIAL_POST_ERROR) {
        res = RUN_STEP_TRIAL_ERROR;
        goto cleanup;
    }

    /* Update seed for next trial */
    *seed = theft_random(t);
    LOG(3 - LOG_RUN, "end of trial, new seed is 0x%016" PRIx64 "\n", *seed);
cleanup:
    theft_trial_free_args(t);
    return res;
}

static uint8_t
infer_arity(const struct theft_run_config *cfg) {
    for (uint8_t i = 0; i < THEFT_MAX_ARITY; i++) {
        if (cfg->type_info[i] == NULL) {
            return i;
        }
    }
    return THEFT_MAX_ARITY;
}

static bool copy_propfun_for_arity(const struct theft_run_config *cfg,
    struct prop_info *prop) {
    switch (prop->arity) {
#define COPY_N(N)                                                     \
        case N:                                                       \
            if (cfg->prop ## N == NULL) {                             \
                return false;                                         \
            } else {                                                  \
                prop->u.fun ## N = cfg->prop ## N;                    \
                break;                                                \
            }

    default:
    case 0:
        assert(false);
        return false;
        COPY_N(1);
        COPY_N(2);
        COPY_N(3);
        COPY_N(4);
        COPY_N(5);
        COPY_N(6);
        COPY_N(7);
#undef COPY_N
    }
    return true;
}

/* Check if all argument info structs have all required callbacks. */
static bool
check_all_args(uint8_t arity, const struct theft_run_config *cfg,
        bool *all_hashable) {
    bool ah = true;
    for (uint8_t i = 0; i < arity; i++) {
        const struct theft_type_info *ti = cfg->type_info[i];
        if (ti->alloc == NULL) { return false; }
        if (ti->autoshrink_config.enable && ti->shrink) { return false; }
        if (ti->hash == NULL && !ti->autoshrink_config.enable) {
            ah = false;
        }
    }
    *all_hashable = ah;
    return true;
}

static bool init_arg_info(struct theft *t, struct trial_info *trial_info) {
    for (size_t i = 0; i < t->prop.arity; i++) {
        const struct theft_type_info *ti = t->prop.type_info[i];
        if (ti->autoshrink_config.enable) {
            trial_info->args[i].type = ARG_AUTOSHRINK;
            trial_info->args[i].u.as.env = theft_autoshrink_alloc_env(t, i, ti);
            if (trial_info->args[i].u.as.env == NULL) {
                return false;
            }
        } else {
            trial_info->args[i].type = ARG_BASIC;
        }
    }
    return true;
}

/* Attempt to instantiate arguments, starting with the current seed. */
static enum all_gen_res
gen_all_args(struct theft *t) {
    for (uint8_t i = 0; i < t->prop.arity; i++) {
        struct theft_type_info *ti = t->prop.type_info[i];
        void *p = NULL;

        enum theft_alloc_res res = (ti->autoshrink_config.enable
            ? theft_autoshrink_alloc(t, t->trial.args[i].u.as.env, &p)
            : ti->alloc(t, ti->env, &p));

        if (res == THEFT_ALLOC_SKIP) {
            return ALL_GEN_SKIP;
        } else if (res == THEFT_ALLOC_ERROR) {
            return ALL_GEN_ERROR;
        } else {
            t->trial.args[i].instance = p;
            LOG(3 - LOG_RUN, "%s: arg %u -- %p\n",
                __func__, i, p);
        }
    }

    /* check bloom filter */
    if (t->bloom && theft_call_check_called(t)) {
        return ALL_GEN_DUP;
    }

    return ALL_GEN_OK;
}

static void free_print_trial_result_env(struct theft *t) {
    if (t->hooks.trial_post == theft_hook_trial_post_print_result
        && t->print_trial_result_env != NULL) {
        free(t->print_trial_result_env);
        t->print_trial_result_env = NULL;
    }
}
