#include "theft_trial_internal.h"

#include <inttypes.h>
#include <assert.h>

#include "theft_call.h"
#include "theft_shrink.h"

/* Now that arguments have been generated, run the trial and update
 * counters, call cb with results, etc. */
bool
theft_trial_run(struct theft *t, struct theft_run_info *run_info,
        struct theft_trial_info *trial_info,
        enum theft_hook_res *cres) {
    void **args = trial_info->args;
    assert(args);
    assert(trial_info->arity > 0);

    if (t->bloom) { theft_call_mark_called(t, run_info, args); }
    enum theft_trial_res tres = theft_call(run_info, args);

    struct theft_hook_info hook_info = {
        .prop_name = run_info->name,
        .total_trials = run_info->trial_count,
        .run_seed = run_info->run_seed,

        .type = THEFT_HOOK_TYPE_TRIAL_POST,
        .u.trial_post = {
            .trial_id = trial_info->trial,
            .trial_seed = trial_info->seed,
            .arity = run_info->arity,
            .args = args,
            .result = tres,
        },
    };

    switch (tres) {
    case THEFT_TRIAL_PASS:
        run_info->pass++;
        *cres = run_info->hook_cb(&hook_info, run_info->env);
        break;
    case THEFT_TRIAL_FAIL:
        if (theft_shrink(t, run_info, trial_info) != SHRINK_OK) {
            hook_info.u.trial_post.result = THEFT_TRIAL_ERROR;
            *cres = run_info->hook_cb(&hook_info, run_info->env);
            return false;
        }
        run_info->fail++;
        *cres = report_on_failure(t, run_info, trial_info, &hook_info);
        break;
    case THEFT_TRIAL_SKIP:
        run_info->skip++;
        *cres = run_info->hook_cb(&hook_info, run_info->env);
        break;
    case THEFT_TRIAL_DUP:
        /* user callback should not return this; fall through */
    case THEFT_TRIAL_ERROR:
        *cres = run_info->hook_cb(&hook_info, run_info->env);
        theft_trial_free_args(run_info, args);
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
static enum theft_hook_res
report_on_failure(struct theft *t,
        struct theft_run_info *run_info,
        struct theft_trial_info *trial_info,
        struct theft_hook_info *hook_info) {
    int arity = run_info->arity;
    fprintf(t->out, "\n\n -- Counter-Example: %s\n",
        run_info->name ? run_info-> name : "");
    fprintf(t->out, "    Trial %u, Seed 0x%016" PRIx64 "\n",
        trial_info->trial, (uint64_t)trial_info->seed);
    for (int i = 0; i < arity; i++) {
        struct theft_type_info *ti = run_info->type_info[i];
        if (ti->print) {
            fprintf(t->out, "    Argument %d:\n", i);
            ti->print(t->out, trial_info->args[i], ti->env);
            fprintf(t->out, "\n");
        }
    }

    enum theft_hook_res res;
    res = run_info->hook_cb(hook_info, run_info->env);
    if (res == THEFT_HOOK_REPEAT_ONCE) {
        res = THEFT_HOOK_REPEAT;
    }
    while (res == THEFT_HOOK_REPEAT) {
        enum theft_trial_res tres = theft_call(run_info, trial_info->args);
        if (tres == THEFT_TRIAL_FAIL) {
            res = run_info->hook_cb(hook_info, run_info->env);
        } else if (tres == THEFT_TRIAL_PASS) {
            fprintf(t->out, "Warning: Failed property passed when re-run.\n");
            res = THEFT_HOOK_ERROR;
        } else if (tres == THEFT_TRIAL_ERROR) {
            return THEFT_HOOK_ERROR;
        } else {
            return THEFT_HOOK_CONTINUE;
        }
    }
    return res;
}
