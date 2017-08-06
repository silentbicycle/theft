#include "theft_trial_internal.h"

#include <inttypes.h>
#include <assert.h>

#include "theft_call.h"
#include "theft_shrink.h"
#include "theft_autoshrink.h"

/* Now that arguments have been generated, run the trial and update
 * counters, call cb with results, etc. */
bool
theft_trial_run(struct theft *t,
        struct theft_trial_info *trial_info,
        enum theft_hook_trial_post_res *tpres) {
    assert(trial_info->args);
    assert(trial_info->arity > 0);

    /* Get the actual arguments, which may be boxed when autoshrinking. */
    void *real_args[THEFT_MAX_ARITY];
    theft_autoshrink_get_real_args(t, real_args, trial_info->args);

    if (t->bloom) {
        theft_call_mark_called(t, trial_info->args);
    }

    bool repeated = false;
    enum theft_trial_res tres = theft_call(t, real_args);
    theft_hook_trial_post_cb *trial_post = t->hooks.trial_post;
    void *trial_post_env = (trial_post == theft_hook_trial_post_print_result
        ? t->print_trial_result_env
        : t->hooks.env);

    struct theft_hook_trial_post_info hook_info = {
        .t = t,
        .prop_name = t->prop.name,
        .total_trials = t->prop.trial_count,
        .run_seed = t->seeds.run_seed,
        .trial_id = trial_info->trial,
        .trial_seed = trial_info->seed,
        .arity = t->prop.arity,
        .args = real_args,
        .result = tres,
    };

    switch (tres) {
    case THEFT_TRIAL_PASS:
        if (!repeated) {
            t->counters.pass++;
        }
        *tpres = trial_post(&hook_info, trial_post_env);
        break;
    case THEFT_TRIAL_FAIL:
        if (!theft_shrink(t, trial_info)) {
            hook_info.result = THEFT_TRIAL_ERROR;
            /* We may not have a valid reference to the arguments
             * anymore, so remove the stale pointers. */
            for (size_t i = 0; i < trial_info->arity; i++) {
                hook_info.args[i] = NULL;
            }
            *tpres = trial_post(&hook_info, trial_post_env);
            return false;
        }

        theft_autoshrink_get_real_args(t, hook_info.args, trial_info->args);
        if (!repeated) {
            t->counters.fail++;
        }
        *tpres = report_on_failure(t, trial_info,
            &hook_info, trial_post, trial_post_env);
        break;
    case THEFT_TRIAL_SKIP:
        if (!repeated) {
            t->counters.skip++;
        }
        *tpres = trial_post(&hook_info, trial_post_env);
        break;
    case THEFT_TRIAL_DUP:
        /* user callback should not return this; fall through */
    case THEFT_TRIAL_ERROR:
        *tpres = trial_post(&hook_info, trial_post_env);
        return false;
    }

    if (*tpres == THEFT_HOOK_TRIAL_POST_ERROR) {
        return false;
    }

    return true;
}

void theft_trial_free_args(struct theft *t, void **args) {
    for (uint8_t i = 0; i < t->prop.arity; i++) {
        struct theft_type_info *ti = t->prop.type_info[i];
        if (ti->free && args[i] != NULL) {
            ti->free(args[i], ti->env);
        }
    }
}

/* Print info about a failure. */
static enum theft_hook_trial_post_res
report_on_failure(struct theft *t,
        struct theft_trial_info *trial_info,
        struct theft_hook_trial_post_info *hook_info,
        theft_hook_trial_post_cb *trial_post,
        void *trial_post_env) {
    theft_hook_counterexample_cb *counterexample = t->hooks.counterexample;
    if (counterexample != NULL) {
        struct theft_hook_counterexample_info hook_info = {
            .t = t,
            .prop_name = t->prop.name,
            .total_trials = t->prop.trial_count,
            .trial_id = trial_info->trial,
            .trial_seed = trial_info->seed,
            .arity = t->prop.arity,
            .type_info = t->prop.type_info,
            /* Note: intentionally NOT using real_args here, because
             * autoshrink_print expects the wrapped version. */
            .args = trial_info->args,
        };
        if (counterexample(&hook_info, t->hooks.env)
            != THEFT_HOOK_COUNTEREXAMPLE_CONTINUE) {
            return THEFT_HOOK_TRIAL_POST_ERROR;
        }
    }

    enum theft_hook_trial_post_res res;
    res = trial_post(hook_info, trial_post_env);

    while (res == THEFT_HOOK_TRIAL_POST_REPEAT
        || res == THEFT_HOOK_TRIAL_POST_REPEAT_ONCE) {
        void *real_args[THEFT_MAX_ARITY];
        theft_autoshrink_get_real_args(t, real_args, trial_info->args);
        enum theft_trial_res tres = theft_call(t, real_args);
        if (tres == THEFT_TRIAL_FAIL) {
            res = trial_post(hook_info, t->hooks.env);
            if (res == THEFT_HOOK_TRIAL_POST_REPEAT_ONCE) {
                break;
            }
        } else if (tres == THEFT_TRIAL_PASS) {
            fprintf(t->out, "Warning: Failed property passed when re-run.\n");
            res = THEFT_HOOK_TRIAL_POST_ERROR;
        } else if (tres == THEFT_TRIAL_ERROR) {
            return THEFT_HOOK_TRIAL_POST_ERROR;
        } else {
            return THEFT_HOOK_TRIAL_POST_CONTINUE;
        }
    }
    return res;
}
