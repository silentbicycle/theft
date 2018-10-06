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
        enum theft_hook_trial_post_res *tpres) {
    assert(t->prop.arity > 0);

    if (t->bloom) { theft_call_mark_called(t); }

    void *args[THEFT_MAX_ARITY];
    theft_trial_get_args(t, args);

    bool repeated = false;
    enum theft_trial_res tres = theft_call(t, args);
    theft_hook_trial_post_cb *trial_post = t->hooks.trial_post;
    void *trial_post_env = (trial_post == theft_hook_trial_post_print_result
        ? t->print_trial_result_env
        : t->hooks.env);

    struct theft_hook_trial_post_info hook_info = {
        .t = t,
        .prop_name = t->prop.name,
        .total_trials = t->prop.trial_count,
        .run_seed = t->seeds.run_seed,
        .trial_id = t->trial.trial,
        .trial_seed = t->trial.seed,
        .arity = t->prop.arity,
        .args = args,
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
        if (!theft_shrink(t)) {
            hook_info.result = THEFT_TRIAL_ERROR;
            /* We may not have a valid reference to the arguments
             * anymore, so remove the stale pointers. */
            for (size_t i = 0; i < t->prop.arity; i++) {
                hook_info.args[i] = NULL;
            }
            *tpres = trial_post(&hook_info, trial_post_env);
            return false;
        }

        if (!repeated) {
            t->counters.fail++;
        }

        theft_trial_get_args(t, hook_info.args);
        *tpres = report_on_failure(t, &hook_info, trial_post, trial_post_env);
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

void theft_trial_free_args(struct theft *t) {
    for (size_t i = 0; i < t->prop.arity; i++) {
        struct theft_type_info *ti = t->prop.type_info[i];

        struct arg_info *ai = &t->trial.args[i];
        if (ai->type == ARG_AUTOSHRINK) {
            theft_autoshrink_free_env(t, ai->u.as.env);
        }
        if (ai->instance != NULL && ti->free != NULL) {
            ti->free(t->trial.args[i].instance, ti->env);
        }
    }
}

void
theft_trial_get_args(struct theft *t, void **args) {
    for (size_t i = 0; i < t->prop.arity; i++) {
        args[i] =  t->trial.args[i].instance;
    }
}

/* Print info about a failure. */
static enum theft_hook_trial_post_res
report_on_failure(struct theft *t,
        struct theft_hook_trial_post_info *hook_info,
        theft_hook_trial_post_cb *trial_post,
        void *trial_post_env) {
    theft_hook_counterexample_cb *counterexample = t->hooks.counterexample;
    if (counterexample != NULL) {
        struct theft_hook_counterexample_info counterexample_hook_info = {
            .t = t,
            .prop_name = t->prop.name,
            .total_trials = t->prop.trial_count,
            .trial_id = t->trial.trial,
            .trial_seed = t->trial.seed,
            .arity = t->prop.arity,
            .type_info = t->prop.type_info,
            .args = hook_info->args,
        };

        if (counterexample(&counterexample_hook_info, t->hooks.env)
            != THEFT_HOOK_COUNTEREXAMPLE_CONTINUE) {
            return THEFT_HOOK_TRIAL_POST_ERROR;
        }
    }

    enum theft_hook_trial_post_res res;
    res = trial_post(hook_info, trial_post_env);

    while (res == THEFT_HOOK_TRIAL_POST_REPEAT
        || res == THEFT_HOOK_TRIAL_POST_REPEAT_ONCE) {
        hook_info->repeat = true;

        enum theft_trial_res tres = theft_call(t, hook_info->args);
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
