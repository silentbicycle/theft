#include <string.h>
#include <assert.h>

#include "theft.h"
#include "theft_types_internal.h"
#include "theft_run.h"

#include "theft_bloom.h"
#include "theft_mt.h"

static struct theft_config default_config = {
    .bloom_bits = 0,
};

/* Initialize a theft test runner, with the configuration
 * in CFG. If CFG is NULL, a default will be used.
 *
 * Returns a NULL if malloc fails or the provided configuration
 * is invalid. */
struct theft *theft_init(const struct theft_config *cfg) {
    if (cfg == NULL) {
        cfg = &default_config;
    }

    if ((cfg->bloom_bits != 0 && (cfg->bloom_bits < THEFT_BLOOM_BITS_MIN))
        || (cfg->bloom_bits > THEFT_BLOOM_BITS_MAX)) {
        return NULL;
    }

    struct theft *t = malloc(sizeof(*t));
    if (t == NULL) { return NULL; }
    memset(t, 0, sizeof(*t));

    t->mt = theft_mt_init(DEFAULT_THEFT_SEED);
    if (t->mt == NULL) {
        free(t);
        return NULL;
    } else {
        t->out = stdout;
        t->requested_bloom_bits = cfg->bloom_bits;
        return t;
    }
}

/* Change T's output stream handle to OUT. (Default: stdout.) */
void theft_set_output_stream(struct theft *t, FILE *out) {
    t->out = out;
}

/* Run a series of randomized trials of a property function.
 *
 * Configuration is specified in CFG; many fields are optional.
 * See the type definition in `theft_types.h`. */
enum theft_run_res
theft_run(struct theft *t, struct theft_run_config *cfg) {
    if (t == NULL || cfg == NULL || cfg->fun == NULL) {
        return THEFT_RUN_ERROR_BAD_ARGS;
    }

    return theft_run_trials(t, cfg);
}

/* Free a property-test runner. */
void theft_free(struct theft *t) {
    if (t->bloom) {
        theft_bloom_dump(t->bloom);
        theft_bloom_free(t->bloom);
        t->bloom = NULL;
    }
    theft_mt_free(t->mt);
    free(t);
}
