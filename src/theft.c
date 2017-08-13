#include <string.h>
#include <assert.h>

#include "theft.h"
#include "theft_types_internal.h"
#include "theft_run.h"

#include "theft_bloom.h"
#include "theft_mt.h"

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
