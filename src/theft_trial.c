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
        enum theft_progress_callback_res *cres) {
    void **args = trial_info->args;
    assert(args);
    assert(trial_info->arity > 0);
    if (t->bloom) { theft_call_mark_called(t, run_info, args); }
    enum theft_trial_res tres = theft_call(run_info, args);

    struct theft_progress_info progress_info = {
        .prop_name = run_info->name,
        .total_trials = run_info->trial_count,
        .run_seed = run_info->run_seed,

        .type = THEFT_PROGRESS_TYPE_TRIAL_POST,
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
        *cres = run_info->progress_cb(&progress_info, run_info->env);
        break;
    case THEFT_TRIAL_FAIL:
        if (theft_shrink(t, run_info, trial_info) != SHRINK_OK) {
            progress_info.u.trial_post.result = THEFT_TRIAL_ERROR;
            *cres = run_info->progress_cb(&progress_info, run_info->env);
            return false;
        }
        run_info->fail++;
        *cres = report_on_failure(t, run_info, trial_info, &progress_info);
        break;
    case THEFT_TRIAL_SKIP:
        run_info->skip++;
        *cres = run_info->progress_cb(&progress_info, run_info->env);
        break;
    case THEFT_TRIAL_DUP:
        /* user callback should not return this; fall through */
    case THEFT_TRIAL_ERROR:
        *cres = run_info->progress_cb(&progress_info, run_info->env);
        theft_trial_free_args(run_info, args);
        return false;
    }

    return true;
}

void theft_trial_free_args(struct theft_run_info *run_info,
        void **args) {
    for (int i = 0; i < run_info->arity; i++) {
        theft_free_cb *free_cb = run_info->type_info[i]->free;
        if (free_cb && args[i] != NULL) {
            free_cb(args[i], run_info->env);
        }
    }
}

/* Print info about a failure. */
static enum theft_progress_callback_res
report_on_failure(struct theft *t,
        struct theft_run_info *run_info,
        struct theft_trial_info *trial_info,
        struct theft_progress_info *progress_info) {
    int arity = run_info->arity;
    fprintf(t->out, "\n\n -- Counter-Example: %s\n",
        run_info->name ? run_info-> name : "");
    fprintf(t->out, "    Trial %u, Seed 0x%016" PRIx64 "\n",
        trial_info->trial, (uint64_t)trial_info->seed);
    for (int i = 0; i < arity; i++) {
        theft_print_cb *print = run_info->type_info[i]->print;
        if (print) {
            fprintf(t->out, "    Argument %d:\n", i);
            print(t->out, trial_info->args[i], run_info->env);
            fprintf(t->out, "\n");
        }
    }

    enum theft_progress_callback_res res;
    res = run_info->progress_cb(progress_info, run_info->env);
    if (res == THEFT_PROGRESS_REPEAT_ONCE) {
        res = THEFT_PROGRESS_REPEAT;
    }
    while (res == THEFT_PROGRESS_REPEAT) {
        enum theft_trial_res tres = theft_call(run_info, trial_info->args);
        if (tres == THEFT_TRIAL_FAIL) {
            res = run_info->progress_cb(progress_info, run_info->env);
        } else if (tres == THEFT_TRIAL_PASS) {
            fprintf(t->out, "Warning: Failed property passed when re-run.\n");
            res = THEFT_PROGRESS_ERROR;
        } else if (tres == THEFT_TRIAL_ERROR) {
            return THEFT_PROGRESS_ERROR;
        } else {
            return THEFT_PROGRESS_CONTINUE;
        }
    }
    return res;
}
