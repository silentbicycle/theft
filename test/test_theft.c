#include "test_theft.h"

/* Add all the definitions that need to be in the test runner's main file. */
GREATEST_MAIN_DEFS();

/* These are included to allocate a valid theft handle, but
 * this file is only testing its random number generation
 * and buffering. */
#include "theft_run.h"
#include "test_theft_autoshrink_ll.h"

static enum theft_trial_res unused(struct theft *t, void *arg1) {
    struct ll *v = (struct ll *)arg1;
    (void)t;
    (void)v;
    return THEFT_TRIAL_ERROR;
}

/* Allocate a theft handle with placeholder/no-op arguments. */
struct theft *test_theft_init(void) {
    struct theft *t = NULL;
    struct theft_run_config cfg = {
        /* These aren't actually used, just defined so that
         * theft_run_init doesn't return an error. */
        .prop1 = unused,
        .type_info = { &ll_info },
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
    RUN_SUITE(prng);
    RUN_SUITE(autoshrink);
    RUN_SUITE(aux);
    RUN_SUITE(bloom);
    RUN_SUITE(error);
    RUN_SUITE(integration);
    GREATEST_MAIN_END();        /* display results */
}
