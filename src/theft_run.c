#include "theft_run_internal.h"

#include "theft_bloom.h"
#include "theft_call.h"
#include "theft_trial.h"
#include <string.h>

/* Actually run the trials, with all arguments made explicit. */
enum theft_run_res
theft_run_trials(struct theft *t, struct theft_propfun_info *info,
        int trials, theft_progress_cb *cb, void *env,
        struct theft_trial_report *r) {

    struct theft_trial_report fake_report;
    if (r == NULL) { r = &fake_report; }
    memset(r, 0, sizeof(*r));
    
    infer_arity(info);
    if (info->arity == 0) {
        return THEFT_RUN_ERROR_BAD_ARGS;
    }

    if (t == NULL || info == NULL || info->fun == NULL
        || info->arity == 0) {
        return THEFT_RUN_ERROR_BAD_ARGS;
    }
    
    bool all_hashable = false;
    if (!check_all_args(info, &all_hashable)) {
        return THEFT_RUN_ERROR_MISSING_CALLBACK;
    }

    if (cb == NULL) {
        cb = default_progress_cb;
    }

    /* If all arguments are hashable, then attempt to use
     * a bloom filter to avoid redundant checking. */
    if (all_hashable) {
        if (t->requested_bloom_bits == 0) {
            t->requested_bloom_bits = theft_bloom_recommendation(trials);
        }
        if (t->requested_bloom_bits != THEFT_BLOOM_DISABLE) {
            t->bloom = theft_bloom_init(t->requested_bloom_bits);
        }
    }
    
    theft_seed seed = t->seed;
    theft_seed initial_seed = t->seed;
    int always_seeds = info->always_seed_count;
    if (info->always_seeds == NULL) { always_seeds = 0; }

    void *args[THEFT_MAX_ARITY];
    
    enum theft_progress_callback_res cres = THEFT_PROGRESS_CONTINUE;

    for (int trial = 0; trial < trials; trial++) {
        memset(args, 0x00, sizeof(args));
        if (cres == THEFT_PROGRESS_HALT) { break; }

        /* If any seeds to always run were specified, use those before
         * reverting to the specified starting seed. */
        if (trial < always_seeds) {
            seed = info->always_seeds[trial];
        } else if ((always_seeds > 0) && (trial == always_seeds)) {
            seed = initial_seed;
        }

        struct theft_trial_info ti = {
            .name = info->name,
            .trial = trial,
            .seed = seed,
            .arity = info->arity,
            .args = args
        };

        theft_set_seed(t, seed);
        enum all_gen_res_t gres = gen_all_args(t, info, seed, args, env);
        switch (gres) {
        case ALL_GEN_SKIP:
            /* skip generating these args */
            ti.status = THEFT_TRIAL_SKIP;
            r->skip++;
            cres = cb(&ti, env);
            break;
        case ALL_GEN_DUP:
            /* skip these args -- probably already tried */
            ti.status = THEFT_TRIAL_DUP;
            r->dup++;
            cres = cb(&ti, env);
            break;
        default:
        case ALL_GEN_ERROR:
            /* Error while generating args */
            ti.status = THEFT_TRIAL_ERROR;
            cres = cb(&ti, env);
            return THEFT_RUN_ERROR;
        case ALL_GEN_OK:
            if (!theft_trial_run(t, info, args, cb, env, r, &ti, &cres)) {
                return THEFT_RUN_ERROR;
            }
        }

        theft_trial_free_args(info, args, env);

        /* Restore last known seed and generate next. */
        theft_set_seed(t, seed);
        seed = theft_random(t);
    }

    if (r->fail > 0) {
        return THEFT_RUN_FAIL;
    } else {
        return THEFT_RUN_PASS;
    }
}

static void
infer_arity(struct theft_propfun_info *info) {
    for (int i = 0; i < THEFT_MAX_ARITY; i++) {
        if (info->type_info[i] == NULL) {
            info->arity = i;
            break;
        }
    }
}

/* Attempt to instantiate arguments, starting with the current seed. */
static enum all_gen_res_t
gen_all_args(struct theft *t, struct theft_propfun_info *info,
        theft_seed seed, void *args[THEFT_MAX_ARITY], void *env) {
    for (int i = 0; i < info->arity; i++) {
        struct theft_type_info *ti = info->type_info[i];
        void *p = NULL;
        enum theft_alloc_res res = ti->alloc(t, seed, env, &p);
        if (res == THEFT_ALLOC_SKIP || res == THEFT_ALLOC_ERROR) {
            for (int j = 0; j < i; j++) {
                ti->free(args[j], env);
            }
            if (res == THEFT_ALLOC_SKIP) {
                return ALL_GEN_SKIP;
            } else {
                return ALL_GEN_ERROR;
            }
        } else {
            args[i] = p;
        }
        seed = theft_random(t);
    }

    /* check bloom filter */
    if (t->bloom && theft_call_check_called(t, info, args, env)) {
        return ALL_GEN_DUP;
    }
    
    return ALL_GEN_OK;
}

/* Check if all argument info structs have all required callbacks. */
static bool
check_all_args(struct theft_propfun_info *info, bool *all_hashable) {
    bool ah = true;
    for (int i = 0; i < info->arity; i++) {
        struct theft_type_info *ti = info->type_info[i];
        if (ti->alloc == NULL) { return false; }
        if (ti->hash == NULL) { ah = false; }
    }
    *all_hashable = ah;
    return true;
}

static enum theft_progress_callback_res
default_progress_cb(struct theft_trial_info *info, void *env) {
    (void)info;
    (void)env;
    return THEFT_PROGRESS_CONTINUE;
}
