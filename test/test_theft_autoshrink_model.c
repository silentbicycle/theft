#include "test_theft.h"
#include "theft_autoshrink.h"
#include "theft_run.h"

static struct theft *t;
static struct autoshrink_model *m;

#define DEF_LIMIT 100000

static void setup(void *unused) {
    (void)unused;
    /* TODO: set seed off time */
    t = test_theft_init();
    m = theft_autoshrink_model_new(t);
}

static void teardown(void *unused) {
    (void)unused;
    if (m) { 
        theft_autoshrink_model_free(t, m);
        m = NULL;
    }
    if (t) {
        theft_run_free(t);
        t = NULL;
    }
}

static uint64_t test_prng(uint8_t bits, void *prng_udata) {
    (void)prng_udata;
    return theft_random_bits(t, bits);
}

TEST successful_drops_should_increase_drop_weight(void) {
    const size_t limit = DEF_LIMIT;
    size_t drops_before = 0;
    for (size_t i = 0; i < limit; i++) {
        if (theft_autoshrink_model_should_drop(test_prng, NULL, m)) {
            drops_before++;
        }
    }

    theft_autoshrink_model_notify_new_trial(m);
    theft_autoshrink_model_notify_attempted(m, ASA_DROP, true);
    theft_autoshrink_model_notify_trial_result(m, THEFT_TRIAL_FAIL);

    size_t drops_after = 0;
    for (size_t i = 0; i < limit; i++) {
        if (theft_autoshrink_model_should_drop(test_prng, NULL, m)) {
            drops_after++;
        }
    }

    if (greatest_get_verbosity() > 0) {
        printf("%s: before %zd, after %zd\n",
            __func__, drops_before, drops_after);
    }
    ASSERT(drops_after > drops_before);
    PASS();
}

TEST reducing_request_count_should_decrease_drop_weight(void) {
    const size_t limit = DEF_LIMIT;
    size_t drops_before = 0;
    for (size_t i = 0; i < limit; i++) {
        if (theft_autoshrink_model_should_drop(test_prng, NULL, m)) {
            drops_before++;
        }
    }

    theft_autoshrink_model_notify_new_trial(m);
    theft_autoshrink_model_notify_attempted(m, ASA_DROP, true);
    theft_autoshrink_model_notify_trial_result(m, THEFT_TRIAL_PASS);

    size_t drops_after = 0;
    for (size_t i = 0; i < limit; i++) {
        if (theft_autoshrink_model_should_drop(test_prng, NULL, m)) {
            drops_after++;
        }
    }

    if (greatest_get_verbosity() > 0) {
        printf("%s: before %zd, after %zd\n",
            __func__, drops_before, drops_after);
    }
    ASSERT(drops_after < drops_before);
    PASS();
}

TEST useless_drop_attempts_should_decrease_drop_weight(void) {
    const size_t limit = DEF_LIMIT;
    size_t drops_before = 0;
    for (size_t i = 0; i < limit; i++) {
        if (theft_autoshrink_model_should_drop(test_prng, NULL, m)) {
            drops_before++;
        }
    }

    theft_autoshrink_model_notify_new_trial(m);
    theft_autoshrink_model_notify_attempted(m, ASA_DROP, false);

    size_t drops_after = 0;
    for (size_t i = 0; i < limit; i++) {
        if (theft_autoshrink_model_should_drop(test_prng, NULL, m)) {
            drops_after++;
        }
    }

    if (greatest_get_verbosity() > 0) {
        printf("%s: before %zd, after %zd\n",
            __func__, drops_before, drops_after);
    }
    ASSERT(drops_after < drops_before);
    PASS();
}

static enum mutation
mutation_of_action(enum autoshrink_action action) {
    switch (action) {
    default:
        assert(false);
    case ASA_SHIFT: return MUT_SHIFT;
    case ASA_MASK: return MUT_MASK;
    case ASA_SWAP: return MUT_SWAP;
    case ASA_SUB: return MUT_SUB;
    }
}

static size_t after[LAST_MUTATION + 1];
static size_t before[LAST_MUTATION + 1];

static void draw_mutations(size_t limit, struct theft *t,
        struct autoshrink_model *m, size_t *counts) {
    for (size_t i = 0; i <= LAST_MUTATION; i++) {
        counts[i] = 0;
    }
    for (size_t i = 0; i < limit; i++) {
        counts[theft_autoshrink_model_get_weighted_mutation(t, m)]++;
    }
}

static void print_counts(const char *label, size_t *counts) {
    printf("==== %s: ", label);
    printf("shift %zd, mask %zd, swap %zd, sub %zd\n",
        counts[MUT_SHIFT],
        counts[MUT_MASK],
        counts[MUT_SWAP],
        counts[MUT_SUB]);
}

TEST success_should_increase_weight(enum autoshrink_action action) {
    const size_t limit = DEF_LIMIT;
    const enum mutation mut = mutation_of_action(action);

    draw_mutations(limit, t, m, before);

    theft_autoshrink_model_notify_new_trial(m);
    theft_autoshrink_model_notify_attempted(m, action, true);
    theft_autoshrink_model_notify_trial_result(m, THEFT_TRIAL_FAIL);

    draw_mutations(limit, t, m, after);

    if (greatest_get_verbosity() > 0) {
        print_counts("before", before);
        print_counts("after", after);
    }
    ASSERT(after[mut] > before[mut]);
    PASS();
}

TEST failed_shrink_should_decrease_weight(enum autoshrink_action action) {
    const size_t limit = DEF_LIMIT;
    const enum mutation mut = mutation_of_action(action);

    draw_mutations(limit, t, m, before);

    theft_autoshrink_model_notify_new_trial(m);
    theft_autoshrink_model_notify_attempted(m, action, true);
    theft_autoshrink_model_notify_trial_result(m, THEFT_TRIAL_PASS);

    draw_mutations(limit, t, m, after);

    if (greatest_get_verbosity() > 0) {
        print_counts("before", before);
        print_counts("after", after);
    }
    ASSERT(after[mut] < before[mut]);
    PASS();
}

TEST skipped_shrink_should_decrease_weight(enum autoshrink_action action) {
    const size_t limit = DEF_LIMIT;
    const enum mutation mut = mutation_of_action(action);

    draw_mutations(limit, t, m, before);

    theft_autoshrink_model_notify_new_trial(m);
    theft_autoshrink_model_notify_attempted(m, action, true);
    theft_autoshrink_model_notify_trial_result(m, THEFT_TRIAL_SKIP);

    draw_mutations(limit, t, m, after);

    if (greatest_get_verbosity() > 0) {
        print_counts("before", before);
        print_counts("after", after);
    }
    ASSERT(after[mut] < before[mut]);
    PASS();
}

TEST useless_attempt_should_decrease_weight(enum autoshrink_action action) {
    const size_t limit = DEF_LIMIT;
    const enum mutation mut = mutation_of_action(action);

    draw_mutations(limit, t, m, before);

    /* tried but not used */
    theft_autoshrink_model_notify_new_trial(m);
    theft_autoshrink_model_notify_attempted(m, action, false);

    draw_mutations(limit, t, m, after);

    if (greatest_get_verbosity() > 0) {
        print_counts("before", before);
        print_counts("after", after);
    }
    ASSERT(after[mut] < before[mut]);
    PASS();
}

SUITE(autoshrink_model) {
    SET_SETUP(setup, NULL);
    SET_TEARDOWN(teardown, NULL);

    /* TODO: loop these with differing seeds */

    RUN_TEST(successful_drops_should_increase_drop_weight);
    RUN_TEST(reducing_request_count_should_decrease_drop_weight);
    RUN_TEST(useless_drop_attempts_should_decrease_drop_weight);

    enum autoshrink_action actions[] = {
        ASA_SHIFT, ASA_MASK, ASA_SWAP, ASA_SUB, 
    };
    
    volatile size_t i = 0;
    for (i = 0; i < sizeof(actions)/sizeof(actions[0]); i++) {
        enum autoshrink_action action = actions[i];
        RUN_TESTp(success_should_increase_weight, action);
        RUN_TESTp(failed_shrink_should_decrease_weight, action);
        RUN_TESTp(skipped_shrink_should_decrease_weight, action);
        RUN_TESTp(useless_attempt_should_decrease_weight, action);
    }
}
