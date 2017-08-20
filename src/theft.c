#include <string.h>
#include <assert.h>

#include "theft.h"
#include "theft_types_internal.h"
#include "theft_run.h"

static enum theft_trial_res should_not_run(struct theft *t, void *arg1);

/* Change T's output stream handle to OUT. (Default: stdout.) */
void theft_set_output_stream(struct theft *t, FILE *out) {
    t->out = out;
}

/* Run a series of randomized trials of a property function.
 *
 * Configuration is specified in CFG; many fields are optional.
 * See the type definition in `theft_types.h`. */
enum theft_run_res
theft_run(const struct theft_run_config *cfg) {
    if (cfg == NULL) {
        return THEFT_RUN_ERROR_BAD_ARGS;
    }

    struct theft *t = NULL;

    enum theft_run_init_res init_res = theft_run_init(cfg, &t);
    switch (init_res) {
    case THEFT_RUN_INIT_ERROR_MEMORY:
        return THEFT_RUN_ERROR_MEMORY;
    default:
        assert(false);
    case THEFT_RUN_INIT_ERROR_BAD_ARGS:
        return THEFT_RUN_ERROR_BAD_ARGS;
    case THEFT_RUN_INIT_OK:
        break;                  /* continue below */
    }

    enum theft_run_res res = theft_run_trials(t);
    theft_run_free(t);
    return res;
}

enum theft_generate_res
theft_generate(FILE *f, theft_seed seed,
        const struct theft_type_info *info, void *hook_env) {
    enum theft_generate_res res = THEFT_GENERATE_OK;
    struct theft *t = NULL;

    struct theft_run_config cfg = {
        .name = "generate",
        .prop1 = should_not_run,
        .type_info = { info },
        .seed = seed,
        .hooks = {
            .env = hook_env,
        },
    };

    enum theft_run_init_res init_res = theft_run_init(&cfg, &t);
    switch (init_res) {
    case THEFT_RUN_INIT_ERROR_MEMORY:
        return THEFT_GENERATE_ERROR_MEMORY;
    default:
        assert(false);
    case THEFT_RUN_INIT_ERROR_BAD_ARGS:
        return THEFT_GENERATE_ERROR_BAD_ARGS;
    case THEFT_RUN_INIT_OK:
        break;                  /* continue below */
    }

    void *instance = NULL;
    enum theft_alloc_res ares = info->alloc(t, info->env, &instance);
    switch (ares) {
    case THEFT_ALLOC_OK:
        break;                  /* continue below */
    case THEFT_ALLOC_SKIP:
        res = THEFT_GENERATE_SKIP;
        goto cleanup;
    case THEFT_ALLOC_ERROR:
        res = THEFT_GENERATE_ERROR_ALLOC;
        goto cleanup;
    }

    if (info->print) {
        fprintf(f, "-- Seed 0x%016" PRIx64 "\n", seed);
        info->print(f, instance, info->env);
        fprintf(f, "\n");
    }
    if (info->free) { info->free(instance, info->env); }

cleanup:
    theft_run_free(t);
    return res;
}

static enum theft_trial_res should_not_run(struct theft *t, void *arg1) {
    (void)t;
    (void)arg1;
    return THEFT_TRIAL_ERROR;   /* should never be run */
}
