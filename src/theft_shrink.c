#include "theft_shrink_internal.h"
#include "theft_shrink.h"

#include "theft_call.h"
#include "theft_trial.h"
#include "theft_autoshrink.h"
#include <assert.h>

#define LOG_SHRINK 0

/* Attempt to simplify all arguments, breadth first. Continue as long as
 * progress is made, i.e., until a local minimum is reached. */
bool
theft_shrink(struct theft *t) {
    bool progress = false;
    assert(t->prop.arity > 0);

    do {
        progress = false;
        /* Greedily attempt to simplify each argument as much as
         * possible before switching to the next. */
        for (uint8_t arg_i = 0; arg_i < t->prop.arity; arg_i++) {
            struct theft_type_info *ti = t->prop.type_info[arg_i];
        greedy_continue:
            if (ti->shrink || ti->autoshrink_config.enable) {
                /* attempt to simplify this argument by one step */
                enum shrink_res rres = attempt_to_shrink_arg(t, arg_i);

                switch (rres) {
                case SHRINK_OK:
                    LOG(3 - LOG_SHRINK, "%s %u: progress\n", __func__, arg_i);
                    progress = true;
                    goto greedy_continue; /* keep trying to shrink same argument */
                case SHRINK_HALT:
                    LOG(3 - LOG_SHRINK, "%s %u: HALT\n", __func__, arg_i);
                    return true;
                case SHRINK_DEAD_END:
                    LOG(3 - LOG_SHRINK, "%s %u: DEAD END\n", __func__, arg_i);
                    continue;   /* try next argument, if any */
                default:
                case SHRINK_ERROR:
                    LOG(1 - LOG_SHRINK, "%s %u: ERROR\n", __func__, arg_i);
                    return false;
                }
            }
        }
    } while (progress);
    return true;
}

/* Simplify an argument by trying all of its simplification tactics, in
 * order, and checking whether the property still fails. If it passes,
 * then revert the simplification and try another tactic.
 *
 * If the bloom filter is being used (i.e., if all arguments have hash
 * callbacks defined), then use it to skip over areas of the state
 * space that have probably already been tried. */
static enum shrink_res
attempt_to_shrink_arg(struct theft *t, uint8_t arg_i) {
    struct theft_type_info *ti = t->prop.type_info[arg_i];
    const bool use_autoshrink = ti->autoshrink_config.enable;

    for (uint32_t tactic = 0; tactic < THEFT_MAX_TACTICS; tactic++) {
        LOG(2 - LOG_SHRINK, "SHRINKING arg %u, tactic %u\n", arg_i, tactic);
        void *current = t->trial.args[arg_i].instance;
        void *candidate = NULL;

        enum theft_hook_shrink_pre_res shrink_pre_res;
        shrink_pre_res = shrink_pre_hook(t, arg_i, current, tactic);
        if (shrink_pre_res == THEFT_HOOK_SHRINK_PRE_HALT) {
            return SHRINK_HALT;
        } else if (shrink_pre_res != THEFT_HOOK_SHRINK_PRE_CONTINUE) {
            return SHRINK_ERROR;
        }

        struct autoshrink_env *as_env = NULL;
        struct autoshrink_bit_pool *current_bit_pool = NULL;
        struct autoshrink_bit_pool *candidate_bit_pool = NULL;
        if (use_autoshrink) {
            as_env = t->trial.args[arg_i].u.as.env;
            assert(as_env);
            current_bit_pool = t->trial.args[arg_i].u.as.env->bit_pool;
        }

        enum theft_shrink_res sres = (use_autoshrink
            ? theft_autoshrink_shrink(t, as_env, tactic, &candidate,
                &candidate_bit_pool)
            : ti->shrink(t, current, tactic, ti->env, &candidate));

        LOG(3 - LOG_SHRINK, "%s: tactic %u -> res %d\n", __func__, tactic, sres);

        t->trial.shrink_count++;

        enum theft_hook_shrink_post_res shrink_post_res;
        shrink_post_res = shrink_post_hook(t, arg_i,
            sres == THEFT_SHRINK_OK ? candidate : current,
            tactic, sres);
        if (shrink_post_res != THEFT_HOOK_SHRINK_POST_CONTINUE) {
            if (ti->free) { ti->free(candidate, ti->env); }
            if (candidate_bit_pool) {
                theft_autoshrink_free_bit_pool(t, candidate_bit_pool);
            }
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

        t->trial.args[arg_i].instance = candidate;
        if (use_autoshrink) { as_env->bit_pool = candidate_bit_pool; }

        if (t->bloom) {
            if (theft_call_check_called(t)) {
                LOG(3 - LOG_SHRINK,
                    "%s: already called, skipping\n", __func__);
                if (ti->free) { ti->free(candidate, ti->env); }
                if (use_autoshrink) {
                    as_env->bit_pool = current_bit_pool;
                    theft_autoshrink_free_bit_pool(t, candidate_bit_pool);
                }
                t->trial.args[arg_i].instance = current;
                continue;
            } else {
                theft_call_mark_called(t);
            }
        }

        enum theft_trial_res res;
        bool repeated = false;
        for (;;) {
            void *args[THEFT_MAX_ARITY];
            theft_trial_get_args(t, args);

            res = theft_call(t, args);
            LOG(3 - LOG_SHRINK, "%s: call -> res %d\n", __func__, res);

            if (!repeated) {
                if (res == THEFT_TRIAL_FAIL) {
                    t->trial.successful_shrinks++;
                    theft_autoshrink_update_model(t, arg_i, res, 3);
                } else {
                    t->trial.failed_shrinks++;
                }
            }

            enum theft_hook_shrink_trial_post_res stpres;
            stpres = shrink_trial_post_hook(t, arg_i, args, tactic, res);
            if (stpres == THEFT_HOOK_SHRINK_TRIAL_POST_REPEAT
                || (stpres == THEFT_HOOK_SHRINK_TRIAL_POST_REPEAT_ONCE && !repeated)) {
                repeated = true;
                continue;  // loop and run again
            } else if (stpres == THEFT_HOOK_SHRINK_TRIAL_POST_REPEAT_ONCE && repeated) {
                break;
            } else if (stpres == THEFT_HOOK_SHRINK_TRIAL_POST_CONTINUE) {
                break;
            } else {
                if (ti->free) { ti->free(current, ti->env); }
                if (use_autoshrink && current_bit_pool) {
                    theft_autoshrink_free_bit_pool(t, current_bit_pool);
                }
                return SHRINK_ERROR;
            }
        }

        theft_autoshrink_update_model(t, arg_i, res, 8);

        switch (res) {
        case THEFT_TRIAL_PASS:
        case THEFT_TRIAL_SKIP:
            LOG(2 - LOG_SHRINK, "PASS or SKIP: REVERTING %u: candidate %p (pool %p), back to %p (pool %p)\n",
                arg_i, (void *)candidate, (void *)candidate_bit_pool,
                (void *)current, (void *)current_bit_pool);
            t->trial.args[arg_i].instance = current;
            if (use_autoshrink) {
                theft_autoshrink_free_bit_pool(t, candidate_bit_pool);
                t->trial.args[arg_i].u.as.env->bit_pool = current_bit_pool;
            }
            if (ti->free) { ti->free(candidate, ti->env); }
            break;
        case THEFT_TRIAL_FAIL:
            LOG(2 - LOG_SHRINK, "FAIL: COMMITTING %u: was %p (pool %p), now %p (pool %p)\n",
                arg_i, (void *)current, (void *)current_bit_pool,
                (void *)candidate, (void *)candidate_bit_pool);
            if (use_autoshrink) {
                assert(t->trial.args[arg_i].u.as.env->bit_pool == candidate_bit_pool);
                theft_autoshrink_free_bit_pool(t, current_bit_pool);
            }
            assert(t->trial.args[arg_i].instance == candidate);
            if (ti->free) { ti->free(current, ti->env); }
            return SHRINK_OK;
        default:
        case THEFT_TRIAL_ERROR:
            if (ti->free) { ti->free(current, ti->env); }
            if (use_autoshrink) {
                theft_autoshrink_free_bit_pool(t, current_bit_pool);
            }
            return SHRINK_ERROR;
        }
    }
    (void)t;
    return SHRINK_DEAD_END;
}

static enum theft_hook_shrink_pre_res
shrink_pre_hook(struct theft *t,
        uint8_t arg_index, void *arg, uint32_t tactic) {
    if (t->hooks.shrink_pre != NULL) {
        struct theft_hook_shrink_pre_info hook_info = {
            .prop_name = t->prop.name,
            .total_trials = t->prop.trial_count,
            .failures = t->counters.fail,
            .run_seed = t->seeds.run_seed,
            .trial_id = t->trial.trial,
            .trial_seed = t->trial.seed,
            .arity = t->prop.arity,
            .shrink_count = t->trial.shrink_count,
            .successful_shrinks = t->trial.successful_shrinks,
            .failed_shrinks = t->trial.failed_shrinks,
            .arg_index = arg_index,
            .arg = arg,
            .tactic = tactic,
        };
        return t->hooks.shrink_pre(&hook_info, t->hooks.env);
    } else {
        return THEFT_HOOK_SHRINK_PRE_CONTINUE;
    }
}

static enum theft_hook_shrink_post_res
shrink_post_hook(struct theft *t,
        uint8_t arg_index, void *arg, uint32_t tactic,
        enum theft_shrink_res sres) {
    if (t->hooks.shrink_post != NULL) {
        enum theft_shrink_post_state state;
        switch (sres) {
        case THEFT_SHRINK_OK:
            state = THEFT_SHRINK_POST_SHRUNK; break;
        case THEFT_SHRINK_NO_MORE_TACTICS:
            state = THEFT_SHRINK_POST_DONE_SHRINKING; break;
        case THEFT_SHRINK_DEAD_END:
            state = THEFT_SHRINK_POST_SHRINK_FAILED; break;
        default:
            assert(false);
            return THEFT_HOOK_SHRINK_POST_ERROR;
        }

        struct theft_hook_shrink_post_info hook_info = {
            .prop_name = t->prop.name,
            .total_trials = t->prop.trial_count,
            .run_seed = t->seeds.run_seed,
            .trial_id = t->trial.trial,
            .trial_seed = t->trial.seed,
            .arity = t->prop.arity,
            .shrink_count = t->trial.shrink_count,
            .successful_shrinks = t->trial.successful_shrinks,
            .failed_shrinks = t->trial.failed_shrinks,
            .arg_index = arg_index,
            .arg = arg,
            .tactic = tactic,
            .state = state,
        };
        return t->hooks.shrink_post(&hook_info, t->hooks.env);
    } else {
        return THEFT_HOOK_SHRINK_POST_CONTINUE;
    }
}

static enum theft_hook_shrink_trial_post_res
shrink_trial_post_hook(struct theft *t,
        uint8_t arg_index, void **args, uint32_t last_tactic,
        enum theft_trial_res result) {
    if (t->hooks.shrink_trial_post != NULL) {
        struct theft_hook_shrink_trial_post_info hook_info = {
            .prop_name = t->prop.name,
            .total_trials = t->prop.trial_count,
            .failures = t->counters.fail,
            .run_seed = t->seeds.run_seed,
            .trial_id = t->trial.trial,
            .trial_seed = t->trial.seed,
            .arity = t->prop.arity,
            .shrink_count = t->trial.shrink_count,
            .successful_shrinks = t->trial.successful_shrinks,
            .failed_shrinks = t->trial.failed_shrinks,
            .arg_index = arg_index,
            .args = args,
            .tactic = last_tactic,
            .result = result,
        };
        return t->hooks.shrink_trial_post(&hook_info,
            t->hooks.env);
    } else {
        return THEFT_HOOK_SHRINK_TRIAL_POST_CONTINUE;
    }
}
