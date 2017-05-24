#include "theft.h"
#include "theft_aux.h"
#include "theft_types_internal.h"

#include <assert.h>
#include <sys/time.h>

theft_seed theft_seed_of_time(void) {
    struct timeval tv;
    if (-1 == gettimeofday(&tv, NULL)) {
        return 0;
    }

    return (uint64_t)theft_hash_onepass((const uint8_t *)&tv, sizeof(tv));
}

void theft_generic_free_cb(void *instance, void *env) {
    (void)env;
    free(instance);
}

/* Print a tally marker for a trial result, but if there have been
 * 10 consecutive ones, increase the scale by an order of magnitude. */
static size_t
autoscale_tally(char *buf, size_t buf_size, size_t scale_factor,
        char *name, size_t *cur_scale, char tally, size_t *count) {
    const size_t scale = *cur_scale == 0 ? 1 : *cur_scale;
    const size_t nscale = scale_factor * scale;
    size_t used = 0;
    if (scale > 1 || *count >= nscale) {
        if (*count == nscale) {
            used = snprintf(buf, buf_size, "(%s x %zd)%c",
                name, nscale, tally);
            *cur_scale = nscale;
        } else if ((*count % scale) == 0) {
            used = snprintf(buf, buf_size, "%c", tally);
        } else {
            buf[0] = '\0';  /* truncate -- print nothing */
        }
    } else {
        used = snprintf(buf, buf_size, "%c", tally);
    }
    (*count)++;
    return used;
}

void theft_print_trial_result(
        struct theft_print_trial_result_env *env,
        const struct theft_hook_trial_post_info *info) {
    assert(env);
    assert(info);

    FILE *f = (env->f ? env->f : stdout);
    const uint8_t maxcol = (env->max_column == 0
        ? DEF_MAX_COLUMNS : env->max_column);

    size_t used = 0;
    char buf[64];

    switch (info->result) {
    case THEFT_TRIAL_PASS:
        used = autoscale_tally(buf, sizeof(buf), 100,
            "PASS", &env->scale_pass, '.', &env->consec_pass);
        break;
    case THEFT_TRIAL_FAIL:
        used = snprintf(buf, sizeof(buf), "F");
        env->scale_pass = 1;
        env->consec_pass = 0;
        env->column = 0;
        break;
    case THEFT_TRIAL_SKIP:
        used = autoscale_tally(buf, sizeof(buf), 10,
            "SKIP", &env->scale_skip, 's', &env->consec_skip);
        break;
    case THEFT_TRIAL_DUP:
        used = autoscale_tally(buf, sizeof(buf), 10,
            "DUP", &env->scale_dup, 'd', &env->consec_dup);
        break;
    case THEFT_TRIAL_ERROR:
        used = snprintf(buf, sizeof(buf), "E");
        break;
    default:
        assert(false);
        return;
    }

    if (env->column + used >= maxcol) {
        fprintf(f, "\n");
        env->column = 0;
    }

    fprintf(f, "%s", buf);
    fflush(f);
    env->column += used;
}

void theft_print_run_pre_info(FILE *f,
        const struct theft_hook_run_pre_info *info) {
    fprintf(f, "\n== PROP '%s': %zd trials, seed 0x%016" PRIx64 "\n",
        info->prop_name, info->total_trials,
        info->run_seed);
}

enum theft_hook_run_pre_res
theft_hook_run_pre_print_info(const struct theft_hook_run_pre_info *info,
        void *env) {
    (void)env;
    theft_print_run_pre_info(stdout, info);
    return THEFT_HOOK_RUN_PRE_CONTINUE;
}

void theft_print_run_post_info(FILE *f,
        const struct theft_hook_run_post_info *info) {
    const struct theft_run_report *r = &info->report;
    fprintf(f, "\n== %s '%s': pass %zd, fail %zd, skip %zd, dup %zd\n",
        r->fail > 0 ? "FAIL" : "PASS", info->prop_name,
        r->pass, r->fail, r->skip, r->dup);
}

enum theft_hook_run_post_res
theft_hook_run_post_print_info(const struct theft_hook_run_post_info *info,
        void *env) {
    (void)env;
    theft_print_run_post_info(stdout, info);
    return THEFT_HOOK_RUN_POST_CONTINUE;
}

struct theft_aux_print_trial_result_env {
    FILE *f;                  // 0 -> default of stdout
    const uint8_t max_column; // 0 -> default of DEF_MAX_COLUMNS

    uint8_t column;
    size_t consec_pass;
    size_t consec_fail;
};
