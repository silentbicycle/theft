#include "test_theft.h"
#include "theft_run.h"

/* Add all the definitions that need to be in the test runner's main file. */
GREATEST_MAIN_DEFS();

static enum theft_trial_res unused(struct theft *t, void *v) {
    (void)t;
    (void)v;
    return THEFT_TRIAL_ERROR;
}

static enum theft_alloc_res
unused_alloc(struct theft *t, void *env, void **instance) {
    (void)t; (void)env; (void)instance;
    assert(false);
    return THEFT_ALLOC_ERROR;
}

struct theft_type_info unused_info = {
    .alloc = unused_alloc,
};

struct theft *test_theft_init(void) {
    struct theft *t = NULL;
    struct theft_run_config cfg = {
        .prop1 = unused,
        .type_info = { &unused_info },
    };

    enum theft_run_init_res res = theft_run_init(&cfg, &t);
    if (res == THEFT_RUN_INIT_OK) {
        return t;
    } else {
        return NULL;
    }
}

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();      /* command-line arguments, initialization. */
    RUN_SUITE(autoshrink);
    RUN_SUITE(aux);
    RUN_SUITE(bloom);
    RUN_SUITE(error);
    RUN_SUITE(integration);
    RUN_SUITE(prng);
    GREATEST_MAIN_END();        /* display results */
}
