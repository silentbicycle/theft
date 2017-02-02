#include <string.h>
#include <assert.h>

#include "theft.h"
#include "theft_types_internal.h"
#include "theft_run.h"

#include "theft_bloom.h"
#include "theft_mt.h"

/* Initialize a theft test runner.
 * BLOOM_BITS sets the size of the table used for detecting
 * combinations of arguments that have already been tested.
 * If 0, a default size will be chosen based on trial count.
 * (This will only be used if all property types have hash
 * callbacks defined.) The bloom filter can also be disabled
 * by setting BLOOM_BITS to THEFT_BLOOM_DISABLE.
 * 
 * Returns a NULL if malloc fails or BLOOM_BITS is out of bounds. */
struct theft *theft_init(uint8_t bloom_bits) {
    if ((bloom_bits != 0 && (bloom_bits < THEFT_BLOOM_BITS_MIN))
        || ((bloom_bits > THEFT_BLOOM_BITS_MAX) &&
            bloom_bits != THEFT_BLOOM_DISABLE)) {
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
        t->requested_bloom_bits = bloom_bits;
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
theft_run(struct theft *t, struct theft_config *cfg) {
    if (t == NULL || cfg == NULL) {
        return THEFT_RUN_ERROR_BAD_ARGS;
    }

    struct theft_propfun_info info;
    memset(&info, 0, sizeof(info));
    info.name = cfg->name;
    info.fun = cfg->fun;
    memcpy(info.type_info, cfg->type_info, sizeof(info.type_info));
    info.always_seed_count = cfg->always_seed_count;
    info.always_seeds = cfg->always_seeds;

    if (cfg->seed) {
        theft_set_seed(t, cfg->seed);
    } else {
        theft_set_seed(t, DEFAULT_THEFT_SEED);
    }

    if (cfg->trials == 0) { cfg->trials = THEFT_DEF_TRIALS; }

    return theft_run_trials(t, &info, cfg->trials, cfg->progress_cb,
        cfg->env, cfg->report);
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
