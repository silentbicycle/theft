#include "theft_trial_internal.h"

#include <inttypes.h>
#include <assert.h>

#include "theft_call.h"
#include "theft_shrink.h"
#include "theft_autoshrink.h"

/* Now that arguments have been generated, run the trial and update
 * counters, call cb with results, etc. */
bool
theft_trial_run(struct theft *t, struct theft_run_info *run_info,
        struct theft_trial_info *trial_info,
        enum theft_hook_trial_post_res *tpres) {
    assert(trial_info->args);
    assert(trial_info->arity > 0);

    /* Get the actual arguments, which may be boxed when autoshrinking. */
    void *real_args[THEFT_MAX_ARITY];
    theft_autoshrink_get_real_args(run_info, real_args, trial_info->args);

    if (t->bloom) { theft_call_mark_called(t, run_info, trial_info->args); }
    
    enum theft_trial_res tres = theft_call(run_info, real_args);
    theft_hook_trial_post_cb *trial_post =
      (run_info->hooks.trial_post == NULL
          ? def_trial_post_cb
          : run_info->hooks.trial_post);

    struct theft_hook_trial_post_info hook_info = {
        .prop_name = run_info->name,
        .total_trials = run_info->trial_count,
        .run_seed = run_info->run_seed,
        .trial_id = trial_info->trial,
        .trial_seed = trial_info->seed,
        .arity = run_info->arity,
        .args = real_args,
        .result = tres,
    };

    switch (tres) {
    case THEFT_TRIAL_PASS:
        run_info->pass++;
        *tpres = trial_post(&hook_info, run_info->hooks.env);
        break;
    case THEFT_TRIAL_FAIL:
        if (theft_shrink(t, run_info, trial_info) != SHRINK_OK) {
            hook_info.result = THEFT_TRIAL_ERROR;
            /* We may not have a valid reference to the arguments
             * anymore, so remove the stale pointers. */
            for (size_t i = 0; i < trial_info->arity; i++) {
                hook_info.args[i] = NULL;
            }
            *tpres = trial_post(&hook_info, run_info->hooks.env);
            return false;
        }

        theft_autoshrink_get_real_args(run_info, hook_info.args, trial_info->args);
        run_info->fail++;
        *tpres = report_on_failure(t, run_info, trial_info,
            &hook_info, trial_post);
        break;
    case THEFT_TRIAL_SKIP:
        run_info->skip++;
        *tpres = trial_post(&hook_info, run_info->hooks.env);
        break;
    case THEFT_TRIAL_DUP:
        /* user callback should not return this; fall through */
    case THEFT_TRIAL_ERROR:
        *tpres = trial_post(&hook_info, run_info->hooks.env);
        theft_trial_free_args(run_info, real_args);
        return false;
    }

    return true;
}

void theft_trial_free_args(struct theft_run_info *run_info,
        void **args) {
    for (int i = 0; i < run_info->arity; i++) {
        struct theft_type_info *ti = run_info->type_info[i];
        if (ti->free && args[i] != NULL) {
            ti->free(args[i], ti->env);
        }
    }
}

/* Print info about a failure. */
static enum theft_hook_trial_post_res
report_on_failure(struct theft *t,
        struct theft_run_info *run_info,
        struct theft_trial_info *trial_info,
        struct theft_hook_trial_post_info *hook_info,
        theft_hook_trial_post_cb *trial_post) {

    theft_hook_counterexample_cb *counterexample = run_info->hooks.counterexample;
    if (counterexample != NULL) {
        struct theft_hook_counterexample_info hook_info = {
            .t = t,
            .prop_name = run_info->name,
            .trial_id = trial_info->trial,
            .trial_seed = trial_info->seed,
            .arity = run_info->arity,
            .type_info = run_info->type_info,
            /* Note: NOT using real_args here because autoshrink_print
             * expects the wrapped version. */
            .args = trial_info->args,
        };
        if (counterexample(&hook_info, run_info->hooks.env)
            != THEFT_HOOK_COUNTEREXAMPLE_CONTINUE) {
            return THEFT_HOOK_TRIAL_POST_ERROR;
        }
    }

    enum theft_hook_trial_post_res res;
    res = trial_post(hook_info, run_info->hooks.env);
    if (res == THEFT_HOOK_TRIAL_POST_REPEAT_ONCE) {
        res = THEFT_HOOK_TRIAL_POST_REPEAT;
    }
    while (res == THEFT_HOOK_TRIAL_POST_REPEAT) {
        enum theft_trial_res tres = theft_call(run_info, trial_info->args);
        if (tres == THEFT_TRIAL_FAIL) {
            res = trial_post(hook_info, run_info->hooks.env);
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

enum theft_hook_trial_post_res
def_trial_post_cb(const struct theft_hook_trial_post_info *info, void *udata) {
    (void)info;
    (void)udata;
    return THEFT_HOOK_TRIAL_POST_CONTINUE;
}
