#include "test_theft.h"
#include "theft_random.h"

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

static struct theft *init(void) {
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

TEST prng_should_return_same_series_from_same_seeds(void) {
    theft_seed seeds[8];
    theft_seed values[8][8];

    struct theft *t = init(); ASSERT(t);

    /* Set for deterministic start */
    theft_random_set_seed(t, 0xabad5eed);
    for (int i = 0; i < 8; i++) {
        seeds[i] = theft_random(t);
    }

    /* Populate value tables. */
    for (int s = 0; s < 8; s++) {
        theft_random_set_seed(t, seeds[s]);
        for (int i = 0; i < 8; i++) {
            values[s][i] = theft_random(t);
        }
    }

    /* Check values. */
    for (int s = 0; s < 8; s++) {
        theft_random_set_seed(t, seeds[s]);
        for (int i = 0; i < 8; i++) {
            ASSERT_EQ(values[s][i], theft_random(t));
        }
    }
    theft_run_free(t);
    PASS();
}

TEST basic_sampling(uint64_t limit) {
    struct theft_run_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    struct theft *t = init(); ASSERT(t);

    for (uint64_t seed = 0; seed < limit; seed++) {
        theft_random_set_seed(t, seed);
        uint64_t num = theft_random(t);

        theft_random_set_seed(t, seed);
        uint64_t num2 = theft_random(t);

        ASSERT_EQ_FMT(num, num2, "%" PRIx64);
    }

    theft_run_free(t);
    PASS();
}

TEST bit_sampling_two_bytes(uint64_t limit) {
    struct theft_run_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    struct theft *t = init(); ASSERT(t);

    for (uint64_t seed = 0; seed < limit; seed++) {
        theft_random_set_seed(t, seed);
        uint16_t a = (uint16_t)(theft_random(t) & 0xFFFF);

        theft_random_set_seed(t, seed);
        uint64_t b0 = 0;

        for (uint8_t i = 0; i < 2; i++) {
            uint64_t byte = (uint8_t)theft_random_bits(t, 8);
            b0 |= (byte << (8L*i));
        }
        uint16_t b = (uint16_t)(b0 & 0xFFFF);

        ASSERT_EQ_FMT(a, b, "0x%04x");
    }

    theft_run_free(t);
    PASS();
}

TEST bit_sampling_bytes(uint64_t limit) {
    struct theft_run_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    struct theft *t = init(); ASSERT(t);

    for (uint64_t seed = 0; seed < limit; seed++) {
        theft_random_set_seed(t, seed);
        uint64_t a0 = theft_random(t);
        uint64_t a1 = theft_random(t);

        theft_random_set_seed(t, seed);
        uint64_t b0 = 0;

        for (uint8_t i = 0; i < 8; i++) {
            uint64_t byte = (uint8_t)theft_random_bits(t, 8);
            b0 |= (byte << (8L*i));
        }
        uint64_t b1 = 0;

        for (uint8_t i = 0; i < 8; i++) {
            uint64_t byte = (uint8_t)theft_random_bits(t, 8);
            b1 |= (byte << (8L*i));
        }

        ASSERT_EQ_FMT(a0, b0, "%" PRIu64);
        ASSERT_EQ_FMT(a1, b1, "%" PRIu64);
    }

    theft_run_free(t);
    PASS();
}

TEST bit_sampling_odd_sizes(uint64_t limit) {
    struct theft_run_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    struct theft *t = init(); ASSERT(t);

    for (uint64_t seed = 0; seed < limit; seed++) {
        theft_random_set_seed(t, seed);
        uint64_t a0 = theft_random(t);
        uint64_t a1 = theft_random(t);

        theft_random_set_seed(t, seed);
        uint64_t b_11 = theft_random_bits(t, 11);
        uint64_t b_13 = theft_random_bits(t, 13);
        uint64_t b_15 = theft_random_bits(t, 15);
        uint64_t b_17 = theft_random_bits(t, 17);
        uint64_t b_19 = theft_random_bits(t, 19);

        uint64_t b0 = (0L
                       | (b_11 << 0)
                       | (b_13 << 11)
                       | (b_15 << (11 + 13))
                       | (b_17 << (11 + 13 + 15))
                       | (b_19 << (11 + 13 + 15 + 17)));

        uint64_t b1 = (b_19 >> 8L);
        uint64_t mask_a1 = a1 & ((1L << 11L) - 1);

        // check that first 64 bits and lower 11 of second uint64_t match
        ASSERT_EQ_FMT(a0, b0, "0x%08" PRIx64);
        ASSERT_EQ_FMT(mask_a1, b1, "0x%08" PRIx64);
    }

    theft_run_free(t);
    PASS();
}

TEST seed_with_upper_32_bits_masked_should_produce_different_value(void) {
    uint64_t seed = 0x15a600d64b175eedLL;
    uint64_t values[3];

    struct theft *t = init(); ASSERT(t);

    theft_random_set_seed(t, seed);
    values[0] = theft_random_bits(t, 64);

    theft_random_set_seed(t, seed | 0xFFFFFFFF00000000L);
    values[1] = theft_random_bits(t, 64);

    theft_random_set_seed(t, seed &~ 0xFFFFFFFF00000000L);
    values[2] = theft_random_bits(t, 64);

    ASSERT(values[0] != values[1]);
    ASSERT(values[0] != values[2]);

    theft_run_free(t);
    PASS();
}

#if THEFT_USE_FLOATING_POINT
TEST check_random_choice_0(void) {
    struct theft *t = init(); ASSERT(t);

    const size_t trials = 10000;
    for (size_t i = 0; i < trials; i++) {
        uint64_t v = theft_random_choice(t, 0);
        ASSERT_EQ_FMTm("limit of 0 should always return 0",
            (uint64_t)0, v, "%" PRIu64);
    }

    theft_run_free(t);
    PASS();
}

TEST check_random_choice_distribution__slow(uint64_t limit, float tolerance) {
    struct theft *t = init(); ASSERT(t);

    size_t *counts = calloc(limit, sizeof(size_t));

    const size_t trials = limit < 10000 ? 10000000 : 1000 * limit;
    for (size_t i = 0; i < trials; i++) {
        uint64_t v = theft_random_choice(t, limit);
        ASSERT(v < limit);
        counts[v]++;
    }

    /* Count, if the trials were perfectly evenly distributed */
    size_t even = trials / (double)limit;

    for (size_t i = 0; i < limit; i++) {
        size_t count = counts[i];
        ASSERT_IN_RANGEm("distribution is too uneven",
            even, count, tolerance * even);
    }

    theft_run_free(t);
    free(counts);
    PASS();
}
#endif

SUITE(prng) {
    RUN_TEST(prng_should_return_same_series_from_same_seeds);

    for (volatile size_t limit = 100; limit < 100000; limit *= 10) {
        RUN_TESTp(basic_sampling, limit);
        RUN_TESTp(bit_sampling_two_bytes, limit);
        RUN_TESTp(bit_sampling_bytes, limit);
        RUN_TESTp(bit_sampling_odd_sizes, limit);
    }

    RUN_TEST(seed_with_upper_32_bits_masked_should_produce_different_value);

#if THEFT_USE_FLOATING_POINT
    RUN_TEST(check_random_choice_0);

    for (volatile uint64_t limit = 1; limit < 300; limit++) {
        RUN_TESTp(check_random_choice_distribution__slow, limit, 0.05);
    }

    /* Relax the tolerance for these a bit, because we aren't running
     * enough trials to smooth out the distribution. */
    RUN_TESTp(check_random_choice_distribution__slow, 10000, 0.20);
#endif
}
