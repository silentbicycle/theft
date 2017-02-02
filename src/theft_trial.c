#include "theft_trial_internal.h"

#include <inttypes.h>

#include "theft_call.h"
#include "theft_shrink.h"

/* Now that arguments have been generated, run the trial and update
 * counters, call cb with results, etc. */
bool
theft_trial_run(struct theft *t, struct theft_propfun_info *info,
        void **args, theft_progress_cb *cb, void *env,
        struct theft_trial_report *r, struct theft_trial_info *ti,
        enum theft_progress_callback_res *cres) {
    if (t->bloom) { theft_call_mark_called(t, info, args, env); }
    enum theft_trial_res tres = theft_call(info, args);
    ti->status = tres;
    switch (tres) {
    case THEFT_TRIAL_PASS:
        r->pass++;
        *cres = cb(ti, env);
        break;
    case THEFT_TRIAL_FAIL:
        if (!theft_shrink(t, info, args, env)) {
            ti->status = THEFT_TRIAL_ERROR;
            *cres = cb(ti, env);
            return false;
        }
        r->fail++;
        *cres = report_on_failure(t, info, ti, cb, env);
        break;
    case THEFT_TRIAL_SKIP:
        *cres = cb(ti, env);
        r->skip++;
        break;
    case THEFT_TRIAL_DUP:
        /* user callback should not return this; fall through */
    case THEFT_TRIAL_ERROR:
        *cres = cb(ti, env);
        theft_trial_free_args(info, args, env);
        return false;
    }
    return true;
}

void theft_trial_free_args(struct theft_propfun_info *info,
        void **args, void *env) {
    for (int i = 0; i < info->arity; i++) {
        theft_free_cb *fcb = info->type_info[i]->free;
        if (fcb && args[i] != NULL) {
            fcb(args[i], env);
        }
    }
}

/* Print info about a failure. */
static enum theft_progress_callback_res
report_on_failure(struct theft *t,
        struct theft_propfun_info *info,
        struct theft_trial_info *ti, theft_progress_cb *cb, void *env) {
    enum theft_progress_callback_res cres;

    int arity = info->arity;
    fprintf(t->out, "\n\n -- Counter-Example: %s\n",
        info->name ? info-> name : "");
    fprintf(t->out, "    Trial %u, Seed 0x%016" PRIx64 "\n", ti->trial,
        (uint64_t)ti->seed);
    for (int i = 0; i < arity; i++) {
        theft_print_cb *print = info->type_info[i]->print;
        if (print) {
            fprintf(t->out, "    Argument %d:\n", i);
            print(t->out, ti->args[i], env);
            fprintf(t->out, "\n");
        }
   }

    cres = cb(ti, env);
    return cres;
}
