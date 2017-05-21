#include "theft_shrink_internal.h"

#include "theft_call.h"
#include "theft_autoshrink.h"
#include <assert.h>

#define LOG_SHRINK 0

/* Attempt to simplify all arguments, breadth first. Continue as long as
 * progress is made, i.e., until a local minima is reached. */
enum shrink_res
theft_shrink(struct theft *t,
        struct theft_run_info *run_info,
        struct theft_trial_info *trial_info) {
    bool progress = false;
    assert(trial_info->arity > 0);

    do {
        progress = false;
        /* Greedily attempt to simplify each argument as much as
         * possible before switching to the next. */
        for (uint8_t arg_i = 0; arg_i < run_info->arity; arg_i++) {
            struct theft_type_info *ti = run_info->type_info[arg_i];
            if (ti->shrink) {
                /* attempt to simplify this argument by one step */
                enum shrink_res rres = attempt_to_shrink_arg(t, run_info,
                    trial_info, arg_i);
                switch (rres) {
                case SHRINK_OK:
                    LOG(3 - LOG_SHRINK, "%s %u: progress\n", __func__, arg_i);
                    progress = true;
                    break;
                case SHRINK_HALT:
                    LOG(3 - LOG_SHRINK, "%s %u: HALT\n", __func__, arg_i);
                    return SHRINK_OK;
                case SHRINK_DEAD_END:
                    LOG(3 - LOG_SHRINK, "%s %u: DEAD END\n", __func__, arg_i);
                    continue;
                default:
                case SHRINK_ERROR:
                    LOG(1 - LOG_SHRINK, "%s %u: ERROR\n", __func__, arg_i);
                    return SHRINK_ERROR;
                }
            }
        }
    } while (progress);
    return SHRINK_OK;
}

/* Simplify an argument by trying all of its simplification tactics, in
 * order, and checking whether the property still fails. If it passes,
 * then revert the simplification and try another tactic.
 *
 * If the bloom filter is being used (i.e., if all arguments have hash
 * callbacks defined), then use it to skip over areas of the state
 * space that have probably already been tried. */
static enum shrink_res
attempt_to_shrink_arg(struct theft *t,
        struct theft_run_info *run_info,
        struct theft_trial_info *trial_info, uint8_t arg_i) {
    struct theft_type_info *ti = run_info->type_info[arg_i];

    void **args = trial_info->args;

    for (uint32_t tactic = 0; tactic < THEFT_MAX_TACTICS; tactic++) {
        LOG(2 - LOG_SHRINK, "SHRINKING arg %u, tactic %u\n", arg_i, tactic);
        void *cur = args[arg_i];
        enum theft_shrink_res sres;
        void *candidate = NULL;

        enum theft_hook_shrink_pre_res shrink_pre_res;
        shrink_pre_res = pre_shrink_hook(run_info, trial_info,
            arg_i, cur, tactic);
        if (shrink_pre_res == THEFT_HOOK_SHRINK_PRE_HALT) {
            return SHRINK_HALT;
        } else if (shrink_pre_res != THEFT_HOOK_SHRINK_PRE_CONTINUE) {
            return SHRINK_ERROR;
        }

        sres = ti->shrink(t, cur, tactic, ti->env, &candidate);
        LOG(3 - LOG_SHRINK, "%s: tactic %u -> res %d\n", __func__, tactic, sres);

        trial_info->shrink_count++;

        enum theft_hook_shrink_post_res shrink_post_res;
        shrink_post_res = post_shrink_hook(run_info, trial_info, arg_i,
            sres == THEFT_SHRINK_OK ? candidate : cur,
            tactic, sres == THEFT_SHRINK_NO_MORE_TACTICS);
        if (shrink_post_res != THEFT_HOOK_SHRINK_POST_CONTINUE) {
            return SHRINK_ERROR;
        }

        switch (sres) {
        case THEFT_SHRINK_OK:
            break;
        case THEFT_SHRINK_DEAD_END:
            continue;           /* try next tactic */
        case THEFT_SHRINK_NO_MORE_TACTICS:
            return SHRINK_DEAD_END;
        case THEFT_SHRINK_ERROR:
        default:
            return SHRINK_ERROR;
        }

        args[arg_i] = candidate;
        if (t->bloom) {
            if (theft_call_check_called(t, run_info, args)) {
                if (ti->free) { ti->free(candidate, ti->env); }
                args[arg_i] = cur;
                continue;
            } else {
                theft_call_mark_called(t, run_info, args);
            }
        }

        enum theft_trial_res res;
        bool repeated = false;
        for (;;) {
            void *real_args[THEFT_MAX_ARITY];
            theft_autoshrink_get_real_args(run_info, real_args, trial_info->args);

            res = theft_call(run_info, real_args);
            LOG(3 - LOG_SHRINK, "%s: call -> res %d\n", __func__, res);

            if (!repeated) {
                if (res == THEFT_TRIAL_FAIL) {
                    trial_info->successful_shrinks++;
                } else {
                    trial_info->failed_shrinks++;
                }
            }

            enum theft_hook_shrink_trial_post_res stpres;
            stpres = post_shrink_trial_hook(run_info, trial_info,
                arg_i, args, tactic, res);
            if (stpres == THEFT_HOOK_SHRINK_TRIAL_POST_REPEAT) {
                repeated = true;
                continue;  // loop and run again
            } else if (stpres == THEFT_HOOK_SHRINK_TRIAL_POST_REPEAT_ONCE && !repeated) {
                repeated = true;
                continue;
            } else if (stpres == THEFT_HOOK_SHRINK_TRIAL_POST_REPEAT_ONCE && repeated) {
                break;
            } else if (stpres == THEFT_HOOK_SHRINK_TRIAL_POST_CONTINUE) {
                break;
            } else {
                return SHRINK_ERROR;
            }
        }

        theft_autoshrink_update_model(t, run_info, arg_i, res);

        switch (res) {
        case THEFT_TRIAL_PASS:
        case THEFT_TRIAL_SKIP:
            /* revert */
            args[arg_i] = cur;
            if (ti->free) { ti->free(candidate, ti->env); }
            break;
        case THEFT_TRIAL_FAIL:
            if (ti->free) { ti->free(cur, ti->env); }
            return SHRINK_OK;
        default:
        case THEFT_TRIAL_ERROR:
            return SHRINK_ERROR;
        }
    }
    (void)t;
    return SHRINK_DEAD_END;
}

static enum theft_hook_shrink_pre_res
pre_shrink_hook(struct theft_run_info *run_info,
        struct theft_trial_info *trial_info,
        uint8_t arg_index, void *arg, uint32_t tactic) {
    if (run_info->hooks.shrink_pre != NULL) {
        struct theft_hook_shrink_pre_info hook_info = {
            .prop_name = run_info->name,
            .total_trials = run_info->trial_count,
            .run_seed = run_info->run_seed,
            .trial_id = trial_info->trial,
            .trial_seed = trial_info->seed,
            .arity = run_info->arity,
            .shrink_count = trial_info->shrink_count,
            .successful_shrinks = trial_info->successful_shrinks,
            .failed_shrinks = trial_info->failed_shrinks,
            .arg_index = arg_index,
            .arg = arg,
            .tactic = tactic,
        };
        return run_info->hooks.shrink_pre(&hook_info, run_info->hooks.env);
    } else {
        return THEFT_HOOK_SHRINK_PRE_CONTINUE;
    }
}

static enum theft_hook_shrink_post_res
post_shrink_hook(struct theft_run_info *run_info,
        struct theft_trial_info *trial_info,
        uint8_t arg_index, void *arg, uint32_t tactic, bool done) {
    if (run_info->hooks.shrink_post != NULL) {
        struct theft_hook_shrink_post_info hook_info = {
            .prop_name = run_info->name,
            .total_trials = run_info->trial_count,
            .run_seed = run_info->run_seed,
            .trial_id = trial_info->trial,
            .trial_seed = trial_info->seed,
            .arity = run_info->arity,
            .shrink_count = trial_info->shrink_count,
            .successful_shrinks = trial_info->successful_shrinks,
            .failed_shrinks = trial_info->failed_shrinks,
            .arg_index = arg_index,
            .arg = arg,
            .tactic = tactic,
            .done = done,
        };
        return run_info->hooks.shrink_post(&hook_info, run_info->hooks.env);
    } else {
        return THEFT_HOOK_SHRINK_POST_CONTINUE;
    }
}

static enum theft_hook_shrink_trial_post_res
post_shrink_trial_hook(struct theft_run_info *run_info,
        struct theft_trial_info *trial_info,
        uint8_t arg_index, void **args, uint32_t last_tactic,
        enum theft_trial_res result) {
    if (run_info->hooks.shrink_trial_post != NULL) {
        struct theft_hook_shrink_trial_post_info hook_info = {
            .prop_name = run_info->name,
            .total_trials = run_info->trial_count,
            .run_seed = run_info->run_seed,
            .trial_id = trial_info->trial,
            .trial_seed = trial_info->seed,
            .arity = run_info->arity,
            .shrink_count = trial_info->shrink_count,
            .successful_shrinks = trial_info->successful_shrinks,
            .failed_shrinks = trial_info->failed_shrinks,
            .arg_index = arg_index,
            .args = args,
            .tactic = last_tactic,
            .result = result,
        };
        return run_info->hooks.shrink_trial_post(&hook_info,
            run_info->hooks.env);
    } else {
        return THEFT_HOOK_SHRINK_TRIAL_POST_CONTINUE;
    }
}
