#include "theft_run_internal.h"

#include "theft_bloom.h"
#include "theft_mt.h"
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
    t->prng.mt = theft_mt_init(DEFAULT_THEFT_SEED);
    if (t->prng.mt == NULL) {
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
    enum wrap_any_autoshrinks_res wrap_res = wrap_any_autoshrinks(t);
    switch (wrap_res) {
    case WRAP_ANY_AUTOSHRINKS_ERROR_MEMORY:
        res = THEFT_RUN_INIT_ERROR_MEMORY;
        goto cleanup;
    case WRAP_ANY_AUTOSHRINKS_ERROR_MISUSE:
        res = THEFT_RUN_INIT_ERROR_BAD_ARGS;
        goto cleanup;
    case WRAP_ANY_AUTOSHRINKS_OK:
        break;                  /* continue below */
    }

    /* If all arguments are hashable, then attempt to use
     * a bloom filter to avoid redundant checking. */
    if (all_hashable) {
        t->bloom = theft_bloom_init(NULL);
    }

    if (t->hooks.trial_post == theft_hook_trial_post_print_result) {
        t->print_trial_result_env = calloc(1,
            sizeof(*t->print_trial_result_env));
        if (t->print_trial_result_env == NULL) {
            return THEFT_RUN_ERROR;
        }
        t->print_trial_result_env->tag = THEFT_PRINT_TRIAL_RESULT_ENV_TAG;
    }

    *output = t;
    return res;

cleanup:
    theft_mt_free(t->prng.mt);
    free(t);
    return res;
}

void theft_run_free(struct theft *t) {
    if (t->bloom) {
        theft_bloom_free(t->bloom);
        t->bloom = NULL;
    }
    theft_mt_free(t->prng.mt);

    if (t->print_trial_result_env != NULL) {
        free(t->print_trial_result_env);
    }

    free_any_autoshrink_wrappers(t);

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
        void *args[THEFT_MAX_ARITY];
        enum run_step_res res = run_step(t, trial, args, &seed);
        LOG(3 - LOG_RUN,
            "  -- trial %zd/%zd, new seed 0x%016" PRIx64 "\n",
            trial, limit, seed);
        theft_trial_free_args(t, args);

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
run_step(struct theft *t, size_t trial, void **args, theft_seed *seed) {
    memset(args, 0x00, THEFT_MAX_ARITY * sizeof(args[0]));
    /* If any seeds to always run were specified, use those before
     * reverting to the specified starting seed. */
    const size_t always_seeds = t->seeds.always_seed_count;
    if (trial < always_seeds) {
        *seed = t->seeds.always_seeds[trial];
    } else if ((always_seeds > 0) && (trial == always_seeds)) {
        *seed = t->seeds.run_seed;
    }

    struct theft_trial_info trial_info = {
        .trial = trial,
        .seed = *seed,
        .arity = t->prop.arity,
        .args = args
    };

    theft_hook_gen_args_pre_cb *gen_args_pre = t->hooks.gen_args_pre;
    if (gen_args_pre != NULL) {
        struct theft_hook_gen_args_pre_info hook_info = {
            .prop_name = t->prop.name,
            .total_trials = t->prop.trial_count,
            .failures = t->counters.fail,
            .run_seed = t->seeds.run_seed,
            .trial_id = trial,
            .trial_seed = trial_info.seed,
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

    enum all_gen_res gres = gen_all_args(t, args);
    theft_hook_trial_post_cb *post_cb = t->hooks.trial_post;
    void *hook_env = (t->hooks.trial_post == theft_hook_trial_post_print_result
        ? t->print_trial_result_env
        : t->hooks.env);

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
    case ALL_GEN_SKIP:
        /* skip generating these args */
        LOG(3 - LOG_RUN, "gen -- skip\n");
        t->counters.skip++;
        hook_info.result = THEFT_TRIAL_SKIP;
        pres = post_cb(&hook_info, hook_env);
        break;
    case ALL_GEN_DUP:
        /* skip these args -- probably already tried */
        LOG(3 - LOG_RUN, "gen -- dup\n");
        t->counters.dup++;
        hook_info.result = THEFT_TRIAL_DUP;
        pres = post_cb(&hook_info, hook_env);
        break;
    default:
    case ALL_GEN_ERROR:
        /* Error while generating args */
        LOG(1 - LOG_RUN, "gen -- error\n");
        hook_info.result = THEFT_TRIAL_ERROR;
        pres = post_cb(&hook_info, hook_env);
        return RUN_STEP_GEN_ERROR;
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
                .args = args,
            };
            enum theft_hook_trial_pre_res tpres;
            tpres = t->hooks.trial_pre(&info, t->hooks.env);
            if (tpres == THEFT_HOOK_TRIAL_PRE_HALT) {
                return RUN_STEP_HALT;
            } else if (tpres == THEFT_HOOK_TRIAL_PRE_ERROR) {
                return RUN_STEP_TRIAL_ERROR;
            }
        }

        if (!theft_trial_run(t, &trial_info, &pres)) {
            return RUN_STEP_TRIAL_ERROR;
        }
    }

    if (pres == THEFT_HOOK_TRIAL_POST_ERROR) {
        return RUN_STEP_TRIAL_ERROR;
    }

    /* Update seed for next trial */
    *seed = theft_random(t);
    LOG(3 - LOG_RUN, "end of trial, new seed is 0x%016" PRIx64 "\n", *seed);
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
        if (ti->hash == NULL && !ti->autoshrink_config.enable) {
            ah = false;
        }
    }
    *all_hashable = ah;
    return true;
}

/* Attempt to instantiate arguments, starting with the current seed. */
static enum all_gen_res
gen_all_args(struct theft *t, void *args[THEFT_MAX_ARITY]) {
    for (uint8_t i = 0; i < t->prop.arity; i++) {
        struct theft_type_info *ti = t->prop.type_info[i];
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
            LOG(3 - LOG_RUN, "%s: arg %u -- %p\n",
                __func__, i, p);
        }
    }

    /* check bloom filter */
    if (t->bloom && theft_call_check_called(t, args)) {
        return ALL_GEN_DUP;
    }

    return ALL_GEN_OK;
}

static enum wrap_any_autoshrinks_res
wrap_any_autoshrinks(struct theft *t) {
    enum wrap_any_autoshrinks_res res = WRAP_ANY_AUTOSHRINKS_OK;
    uint8_t wrapped = 0;
    for (uint8_t i = 0; i < t->prop.arity; i++) {
        struct theft_type_info *ti = t->prop.type_info[i];
        if (ti->autoshrink_config.enable) {
            struct theft_type_info *new_ti = calloc(1, sizeof(*new_ti));
            if (new_ti == NULL) {
                res = WRAP_ANY_AUTOSHRINKS_ERROR_MEMORY;
                goto cleanup;
            }
            enum theft_autoshrink_wrap wrap_res;
            wrap_res = theft_autoshrink_wrap(t, ti, new_ti);
            switch (wrap_res) {
            case THEFT_AUTOSHRINK_WRAP_ERROR_MEMORY:
                res = WRAP_ANY_AUTOSHRINKS_ERROR_MEMORY;
                goto cleanup;
            default:
                assert(false);
            case THEFT_AUTOSHRINK_WRAP_ERROR_MISUSE:
                res = WRAP_ANY_AUTOSHRINKS_ERROR_MISUSE;
                goto cleanup;
            case THEFT_AUTOSHRINK_WRAP_OK:
                break;          /* continue below */
            }
            wrapped++;
            t->prop.type_info[i] = new_ti;
        }
    }

    return res;
cleanup:
    for (uint8_t i = 0; i < wrapped; i++) {
        struct theft_type_info *ti = t->prop.type_info[i];
        if (ti->autoshrink_config.enable) {
            struct theft_autoshrink_env *env =
              (struct theft_autoshrink_env *)ti->env;
            assert(env->tag == AUTOSHRINK_ENV_TAG);
            free(env);
            free(ti);
        }
    }
    return res;
}

static void free_any_autoshrink_wrappers(struct theft *t) {
    for (uint8_t i = 0; i < t->prop.arity; i++) {
        struct theft_type_info *ti = t->prop.type_info[i];
        if (ti->autoshrink_config.enable) {
            struct theft_autoshrink_env *env =
              (struct theft_autoshrink_env *)ti->env;
            assert(env->tag == AUTOSHRINK_ENV_TAG);
            free(env);
            free(ti);
        }
    }
}

static void free_print_trial_result_env(struct theft *t) {
    if (t->hooks.trial_post == theft_hook_trial_post_print_result
        && t->print_trial_result_env != NULL) {
        free(t->print_trial_result_env);
        t->print_trial_result_env = NULL;
    }
}
