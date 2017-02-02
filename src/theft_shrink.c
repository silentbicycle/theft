#include "theft_shrink_internal.h"

#include "theft_call.h"

/* Attempt to simplify all arguments, breadth first. Continue as long as
 * progress is made, i.e., until a local minima is reached. */
bool
theft_shrink(struct theft *t, struct theft_propfun_info *info,
        void *args[], void *env) {
    bool progress = false;
    do {
        progress = false;
        for (int ai = 0; ai < info->arity; ai++) {
            struct theft_type_info *ti = info->type_info[ai];
            if (ti->shrink) {
                /* attempt to simplify this argument by one step */
                enum shrink_res rres = attempt_to_shrink_arg(t, info, args, env, ai);
                switch (rres) {
                case SHRINK_OK:
                    progress = true;
                    break;
                case SHRINK_DEAD_END:
                    break;
                default:
                case SHRINK_ERROR:
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
attempt_to_shrink_arg(struct theft *t, struct theft_propfun_info *info,
        void *args[], void *env, int ai) {
    struct theft_type_info *ti = info->type_info[ai];

    for (uint32_t tactic = 0; tactic < THEFT_MAX_TACTICS; tactic++) {
        void *cur = args[ai];
        enum theft_shrink_res sres;
        void *candidate = NULL;
        sres = ti->shrink(cur, tactic, env, &candidate);
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

        args[ai] = candidate;
        if (t->bloom) {
            if (theft_call_check_called(t, info, args, env)) {
                /* probably redundant */
                if (ti->free) { ti->free(candidate, env); }
                args[ai] = cur;
                continue;
            } else {
                theft_call_mark_called(t, info, args, env);
            }
        }
        enum theft_trial_res res = theft_call(info, args);
        
        switch (res) {
        case THEFT_TRIAL_PASS:
        case THEFT_TRIAL_SKIP:
            /* revert */
            args[ai] = cur;
            if (ti->free) { ti->free(candidate, env); }
            break;
        case THEFT_TRIAL_FAIL:
            if (ti->free) { ti->free(cur, env); }
            return SHRINK_OK;
        case THEFT_TRIAL_DUP:  /* user callback should not return this */
        case THEFT_TRIAL_ERROR:
            return SHRINK_ERROR;
        }
    }
    (void)t;
    return SHRINK_DEAD_END;
}

