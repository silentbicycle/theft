#include "test_theft.h"

TEST basic(uint64_t limit) {
    struct theft_run_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    struct theft *t = theft_init(NULL);

    for (uint64_t seed = 0; seed < limit; seed++) {
        theft_set_seed(t, seed);
        uint64_t num = theft_random(t);

        theft_set_seed(t, seed);
        uint64_t num2 = theft_random(t);

        ASSERT_EQ_FMT(num, num2, "%llx");
    }

    theft_free(t);
    PASS();
}

TEST bit_sampling_two_bytes(uint64_t limit) {
    struct theft_run_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    struct theft *t = theft_init(NULL);

    for (uint64_t seed = 0; seed < limit; seed++) {
        theft_set_seed(t, seed);
        uint16_t a = (uint16_t)(theft_random(t) & 0xFFFF);

        theft_set_seed(t, seed);
        uint64_t b0 = 0;

        for (uint8_t i = 0; i < 2; i++) {
            uint64_t byte = (uint8_t)theft_random_bits(t, 8);
            b0 |= (byte << (8L*i));
        }
        uint16_t b = (uint16_t)(b0 & 0xFFFF);

        ASSERT_EQ_FMT(a, b, "0x%04x");
    }

    theft_free(t);
    PASS();
}

TEST bit_sampling_bytes(uint64_t limit) {
    struct theft_run_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    struct theft *t = theft_init(NULL);

    for (uint64_t seed = 0; seed < limit; seed++) {
        theft_set_seed(t, seed);
        uint64_t a0 = theft_random(t);
        uint64_t a1 = theft_random(t);

        theft_set_seed(t, seed);
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

        ASSERT_EQ_FMT(a0, b0, "%llu");
        ASSERT_EQ_FMT(a1, b1, "%llu");
    }

    theft_free(t);
    PASS();
}

TEST bit_sampling_odd_sizes(uint64_t limit) {
    struct theft_run_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    struct theft *t = theft_init(NULL);

    for (uint64_t seed = 0; seed < limit; seed++) {
        theft_set_seed(t, seed);
        uint64_t a0 = theft_random(t);
        uint64_t a1 = theft_random(t);

        theft_set_seed(t, seed);
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
        ASSERT_EQ_FMT(a0, b0, "0x%08x");
        ASSERT_EQ_FMT(mask_a1, b1, "0x%08x");
    }

    theft_free(t);
    PASS();
}

SUITE(prng) {
    for (size_t limit = 100; limit < 100000; limit *= 10) {
        RUN_TESTp(basic, limit);
        RUN_TESTp(bit_sampling_two_bytes, limit);
        RUN_TESTp(bit_sampling_bytes, limit);
        RUN_TESTp(bit_sampling_odd_sizes, limit);
    }
}
