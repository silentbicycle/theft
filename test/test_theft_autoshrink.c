#include "test_theft.h"
#include "theft_autoshrink.h"
#include "test_theft_autoshrink_ll.h"
#include "test_theft_autoshrink_bulk.h"
#include "test_theft_autoshrink_int_array.h"

#include <sys/time.h>

#include "theft_aux.h"

#define MAX_PAIRS 16
struct fake_prng_info {
    size_t pos;
    struct {
        uint8_t bits;
        uint64_t value;
    } pairs[MAX_PAIRS];
};

static uint64_t fake_prng(uint8_t bits, void *udata) {
    struct fake_prng_info *info = (struct fake_prng_info *)udata;
    //printf("BITS, %d\n", bits);
    if (bits == info->pairs[info->pos].bits) {
        return info->pairs[info->pos++].value;
    } else {
        assert(false);
        return 0;
    }
}

static int bit_pool_eq(const void *exp, const void *got, void *udata) {
    (void)udata;
    struct theft_autoshrink_bit_pool *a = (struct theft_autoshrink_bit_pool *)exp;
    struct theft_autoshrink_bit_pool *b = (struct theft_autoshrink_bit_pool *)got;
    if (a->bits_filled != b->bits_filled) { return 0; }
    if (a->consumed != b->consumed) { return 0; }
    if (a->request_count != b->request_count) { return 0; }

    for (size_t i = 0; i < a->request_count; i++) {
        if (a->requests[i] != b->requests[i]) {
            return 0;
        }
    }
    const size_t limit = (a->bits_filled / 8) + ((a->bits_filled % 8) == 0 ? 0 : 1);
    for (size_t i = 0; i < limit; i++) {
        if (a->bits[i] != b->bits[i]) {
            return 0;
        }
    }

    return 1;
}

static int bit_pool_print(const void *t, void *udata) {
    struct theft_autoshrink_bit_pool *pool = (struct theft_autoshrink_bit_pool *)t;
    theft_autoshrink_dump_bit_pool(stdout, pool->bits_filled, pool,
        THEFT_AUTOSHRINK_PRINT_ALL);
    (void)udata;
    return 0;
}

static struct greatest_type_info bit_pool_info = {
    .equal = bit_pool_eq,
    .print = bit_pool_print,
};

/* These bits will construct an LL of {0, 1, 0, 3, 0}:
 * 0b001, 0b00000000,
 * 0b001, 0b00000001,
 * 0b001, 0b00000000,
 * 0b001, 0b00000010,
 * 0b001, 0b00000000,
 * 0b000 (end of list) -- 58 bits
 *
 * 0000 0001 -- 0x01
 * 0100 1000 -- 0x48
 * 0100 0000 -- 0x40
 * 0000 0000 -- 0x00
 * 0011 0010 -- 0x32
 * 0001 0000 -- 0x10
 * 0000 0000 -- 0x00
 * 0000 0000 -- 0x00
 */
static uint8_t test_pool_bits[] = { 0x01, 0x48, 0x40, 0x00, 0x32, 0x10, 0x00, 0x00 };
#define TEST_POOL_BIT_COUNT (5 * (3 + 8) + 3)
static uint32_t test_pool_requests[] = { 3, 8, 3, 8, 3, 8, 3, 8, 3, 8, 3 };
static struct theft_autoshrink_bit_pool test_pool = {
    .tag = AUTOSHRINK_BIT_POOL_TAG,
    .bits = test_pool_bits,
    .bits_filled = TEST_POOL_BIT_COUNT,
    .limit = TEST_POOL_BIT_COUNT,
    .consumed = TEST_POOL_BIT_COUNT,
    .request_count = sizeof(test_pool_requests)/sizeof(test_pool_requests[0]),
    .request_ceil = 999,
    .requests = test_pool_requests,
};

TEST ll_drop_nothing(void) {
    struct theft *t = theft_init(0);

    struct fake_prng_info prng_info = {
        .pairs = {
            { 32, DO_NOT_DROP },
            { 5, 31, },  // don't drop anything
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
        },
    };
    struct theft_autoshrink_env env = {
        .tag = AUTOSHRINK_ENV_TAG,
        .user_type_info = ll_info,
        .prng = fake_prng,
        .udata = &prng_info,
        .leave_trailing_zeroes = true,
    };
    theft_autoshrink_model_set_next(&env, ASA_DROP);

    void *output = NULL;
    enum theft_shrink_res res;
    res = theft_autoshrink_shrink(t, &test_pool, 0, &env, &output);
    ASSERT_EQ_FMT(THEFT_SHRINK_OK, res, "%d");

    struct theft_autoshrink_bit_pool *out = (struct theft_autoshrink_bit_pool *)output;

    uint8_t exp_bits[] = { 0x01, 0x48, 0x40, 0x00, 0x32, 0x10, 0x00, 0x00 };
    uint32_t exp_requests[] = { 3, 8,
                                3, 8,
                                3, 8,
                                3, 8,
                                3, 8,
                                3, };
    struct theft_autoshrink_bit_pool exp_pool = {
        .tag = AUTOSHRINK_BIT_POOL_TAG,
        .bits = exp_bits,
        .bits_filled = TEST_POOL_BIT_COUNT,
        .consumed = 5 * (3 + 8) + 3,
        .request_count = sizeof(exp_requests)/sizeof(exp_requests[0]),
        .request_ceil = 999,
        .requests = exp_requests,
    };

    /* Just drop the zeroes off the end */
    ASSERT_EQUAL_T(&exp_pool, out, &bit_pool_info, NULL);

    ll_info.free(out->instance, NULL);
    out->instance = NULL;
    theft_autoshrink_free_bit_pool(t, out);
    theft_free(t);
    PASS();
}

TEST ll_drop_nothing_but_do_truncate(void) {
    struct theft *t = theft_init(0);

    struct fake_prng_info prng_info = {
        .pairs = {
            { 32, DO_NOT_DROP },
            { 5, 31, },  // don't drop anything
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
        },
    };
    struct theft_autoshrink_env env = {
        .tag = AUTOSHRINK_ENV_TAG,
        .user_type_info = ll_info,
        .prng = fake_prng,
        .udata = &prng_info,
    };
    theft_autoshrink_model_set_next(&env, ASA_DROP);

    void *output = NULL;
    enum theft_shrink_res res;
    res = theft_autoshrink_shrink(t, &test_pool, 0, &env, &output);
    ASSERT_EQ_FMT(THEFT_SHRINK_OK, res, "%d");

    struct theft_autoshrink_bit_pool *out = (struct theft_autoshrink_bit_pool *)output;

    /* Drop the zeroes off the end */
    uint8_t exp_bits[] = { 0x01, 0x48, 0x40, 0x00, 0x32, 0x10, };
    uint32_t exp_requests[] = { 3, 8,
                                3, 8,
                                3, 8,
                                3, 8,
                                3, 1, // last request is truncated
    };
    struct theft_autoshrink_bit_pool exp_pool = {
        .tag = AUTOSHRINK_BIT_POOL_TAG,
        .bits = exp_bits,
        .bits_filled = 8 * sizeof(exp_bits),
        .limit = TEST_POOL_BIT_COUNT,
        .consumed = 4 * (3 + 8) + 3 + 1,
        .request_count = sizeof(exp_requests)/sizeof(exp_requests[0]),
        .request_ceil = 999,
        .requests = exp_requests,
    };

    ASSERT_EQUAL_T(&exp_pool, out, &bit_pool_info, NULL);

    ll_info.free(out->instance, NULL);
    out->instance = NULL;
    theft_autoshrink_free_bit_pool(t, out);
    theft_free(t);
    PASS();
}

TEST ll_drop_first(void) {
    struct theft *t = theft_init(0);

    struct fake_prng_info prng_info = {
        .pairs = {
            { 32, DO_NOT_DROP },
            { 5, 0, },  // drop first 3 bits
            { 5, 0, },  // ... and corresponding 8 bits
            { 5, 31, }, // don't drop the rest
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
        },
    };
    struct theft_autoshrink_env env = {
        .tag = AUTOSHRINK_ENV_TAG,
        .user_type_info = ll_info,
        .prng = fake_prng,
        .udata = &prng_info,
        .leave_trailing_zeroes = true,
    };
    theft_autoshrink_model_set_next(&env, ASA_DROP);

    void *output = NULL;
    enum theft_shrink_res res;
    res = theft_autoshrink_shrink(t, &test_pool, 0, &env, &output);
    ASSERT_EQ_FMT(THEFT_SHRINK_OK, res, "%d");

    struct theft_autoshrink_bit_pool *out = (struct theft_autoshrink_bit_pool *)output;

    /* These bits will construct an LL of {1, 0, 3, 0}:
     * 0b001, 0b00000001,
     * 0b001, 0b00000000,
     * 0b001, 0b00000011,
     * 0b001, 0b00000000,
     * 0b000 (end of list), 47 bits total
     *
     * 0000 1001 -- 0x09
     * 0000 1000 -- 0x08
     * 0100 0000 -- 0x40
     * 0000 0110 -- 0x06
     * 0010 0000 -- 0x20
     * _000 0000 -- 0x00 */
    uint8_t shrunk_bits[] = { 0x09, 0x08, 0x40, 0x06, 0x02, 0x00, 0x00 };
    uint32_t shrunk_requests[] = { 3, 8, 3, 8, 3, 8, 3, 8, 3, };
    struct theft_autoshrink_bit_pool expected = {
        .tag = AUTOSHRINK_BIT_POOL_TAG,
        .bits = shrunk_bits,
        .bits_filled = TEST_POOL_BIT_COUNT - (3 + 8),
        .consumed = 4 * (3 + 8) + 3,
        .request_count = sizeof(shrunk_requests)/sizeof(shrunk_requests[0]),
        .request_ceil = 999,
        .requests = shrunk_requests,
    };

    ASSERT_EQUAL_T(&expected, out, &bit_pool_info, NULL);

    ll_info.free(out->instance, NULL);
    out->instance = NULL;
    theft_autoshrink_free_bit_pool(t, out);

    theft_free(t);
    PASS();
}

TEST ll_drop_third_and_fourth(void) {
    struct theft *t = theft_init(0);

    struct fake_prng_info prng_info = {
        .pairs = {
            { 32, DO_NOT_DROP },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 0, }, // drop third link
            { 5, 0, },
            { 5, 0, }, // drop fourth link
            { 5, 0, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
        },
    };
    struct theft_autoshrink_env env = {
        .tag = AUTOSHRINK_ENV_TAG,
        .user_type_info = ll_info,
        .prng = fake_prng,
        .udata = &prng_info,
        .leave_trailing_zeroes = true,
    };
    theft_autoshrink_model_set_next(&env, ASA_DROP);

    void *output = NULL;
    enum theft_shrink_res res;
    res = theft_autoshrink_shrink(t, &test_pool, 0, &env, &output);
    ASSERT_EQ_FMT(THEFT_SHRINK_OK, res, "%d");

    struct theft_autoshrink_bit_pool *out = (struct theft_autoshrink_bit_pool *)output;

    /* These bits will construct an LL of {0, 1, 0}:
     * 0b001, 0b00000000,
     * 0b001, 0b00000001,
     * 0b001, 0b00000000,
     * 0b000 (end of list), 36 bits total */
    uint8_t exp_bits[] = { 0x01, 0x48, 0x40, 0x00, 0x00, 0x00 };
    uint32_t exp_requests[] = { 3, 8,
                                3, 8,
                                3, 8,
                                3, };
    struct theft_autoshrink_bit_pool exp_pool = {
        .tag = AUTOSHRINK_BIT_POOL_TAG,
        .bits = exp_bits,
        .bits_filled = TEST_POOL_BIT_COUNT - 2*(3 + 8),
        .consumed = 3*(3 + 8) + 3,
        .request_count = sizeof(exp_requests)/sizeof(exp_requests[0]),
        .request_ceil = 999,
        .requests = exp_requests,
    };

    ASSERT_EQUAL_T(&exp_pool, out, &bit_pool_info, NULL);

    ll_info.free(out->instance, NULL);
    out->instance = NULL;
    theft_autoshrink_free_bit_pool(t, out);
    theft_free(t);
    PASS();
}

TEST ll_drop_last(void) {
    struct theft *t = theft_init(0);

    struct fake_prng_info prng_info = {
        .pairs = {
            { 32, DO_NOT_DROP },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 31, },
            { 5, 0, }, // drop last link
            { 5, 0, },
            { 5, 31, },
        },
    };
    struct theft_autoshrink_env env = {
        .tag = AUTOSHRINK_ENV_TAG,
        .user_type_info = ll_info,
        .prng = fake_prng,
        .udata = &prng_info,
        .leave_trailing_zeroes = true,
    };
    theft_autoshrink_model_set_next(&env, ASA_DROP);

    void *output = NULL;
    enum theft_shrink_res res;
    res = theft_autoshrink_shrink(t, &test_pool, 0, &env, &output);
    ASSERT_EQ_FMT(THEFT_SHRINK_OK, res, "%d");

    struct theft_autoshrink_bit_pool *out = (struct theft_autoshrink_bit_pool *)output;

    /* These bits will construct an LL of {0, 1, 0, 3}:
     * 0b001, 0b00000000,
     * 0b001, 0b00000001,
     * 0b001, 0b00000000,
     * 0b001, 0b00000011,
     * 0b000 (end of list), 47 bits total */
    uint8_t exp_bits[] = { 0x01, 0x48, 0x40, 0x00, 0x32, 0x00, };
    uint32_t exp_requests[] = { 3, 8,
                                3, 8,
                                3, 8,
                                3, 8,
                                3, };
    struct theft_autoshrink_bit_pool exp_pool = {
        .tag = AUTOSHRINK_BIT_POOL_TAG,
        .bits = exp_bits,
        .bits_filled = TEST_POOL_BIT_COUNT - (3 + 8),
        .consumed = 4*(3 + 8) + 3,
        .request_count = sizeof(exp_requests)/sizeof(exp_requests[0]),
        .request_ceil = 999,
        .requests = exp_requests,
    };

    ASSERT_EQUAL_T(&exp_pool, out, &bit_pool_info, NULL);

    ll_info.free(out->instance, NULL);
    out->instance = NULL;
    theft_autoshrink_free_bit_pool(t, out);
    theft_free(t);
    PASS();
}

TEST ll_mutate_shift(void) {
    struct theft *t = theft_init(0);

    uint8_t pos_bits = 4; // log2ceil(11)

    struct fake_prng_info prng_info = {
        // three changes, all right shifting by 1
        .pairs = {
            // popcount: 3 changes
            { 5, 0x01 | 0x02 /* + 1 */, },

            // right-shift value for 4th link by 1
            { pos_bits, 7 },
            { 2, 0 },

            // right-shift value for 2th link by 2
            { pos_bits, 3 },
            { 2, 1 },

            // right-shift continue-bits for 5th link by 1
            { pos_bits, 8 },
            { 2, 0 },
        },
    };

    struct theft_autoshrink_env env = {
        .tag = AUTOSHRINK_ENV_TAG,
        .user_type_info = ll_info,
        .prng = fake_prng,
        .udata = &prng_info,
        .leave_trailing_zeroes = true,
    };
    theft_autoshrink_model_set_next(&env, ASA_SHIFT);

    void *output = NULL;
    enum theft_shrink_res res;
    res = theft_autoshrink_shrink(t, &test_pool, 1, &env, &output);
    ASSERT_EQ_FMT(THEFT_SHRINK_OK, res, "%d");

    struct theft_autoshrink_bit_pool *out = (struct theft_autoshrink_bit_pool *)output;

    /* These bits will construct an LL of {0, 0, 0, 1}:
     * 0b001, 0b00000000,
     * 0b001, 0b00000000,  (value is right-shifted 2)
     * 0b001, 0b00000000,
     * 0b001, 0b00000001,  (value is right-shifted 1)
     * 0b000, 0b00000000,  (continue bits are right-shifted 1)
     * 0b000 (end of list) -- 58 bits */
    uint8_t exp_bits[] = { 0x01, 0x08, 0x40, 0x00, 0x12, 0x00, 0x00, 0x00 };
    uint32_t exp_requests[] = { 3, 8,
                                3, 8,
                                3, 8,
                                3, 8,
                                3, };
    struct theft_autoshrink_bit_pool exp_pool = {
        .tag = AUTOSHRINK_BIT_POOL_TAG,
        .bits = exp_bits,
        .bits_filled = TEST_POOL_BIT_COUNT,
        .consumed = 4 * (3 + 8) + 3,
        .request_count = sizeof(exp_requests)/sizeof(exp_requests[0]),
        .request_ceil = 999,
        .requests = exp_requests,
    };

    /* Just drop the zeroes off the end */
    ASSERT_EQUAL_T(&exp_pool, out, &bit_pool_info, NULL);

    ll_info.free(out->instance, NULL);
    out->instance = NULL;
    theft_autoshrink_free_bit_pool(t, out);
    theft_free(t);
    PASS();
}

TEST ll_mutate_mask(void) {
    struct theft *t = theft_init(0);

    uint8_t pos_bits = 4; // log2ceil(11)

    struct fake_prng_info prng_info = {
        .pairs = {
            // popcount: 1 change
            { 5, 0x00 /* + 1 */, },

            // mask for 4th link by 0xfe
            { pos_bits, 7 },
            { 8, 0xf0 },
            { 8, 0x0e },
        },
    };
    struct theft_autoshrink_env env = {
        .tag = AUTOSHRINK_ENV_TAG,
        .user_type_info = ll_info,
        .prng = fake_prng,
        .udata = &prng_info,
        .leave_trailing_zeroes = true,
    };
    theft_autoshrink_model_set_next(&env, ASA_MASK);

    void *output = NULL;
    enum theft_shrink_res res;
    res = theft_autoshrink_shrink(t, &test_pool, 1, &env, &output);
    ASSERT_EQ_FMT(THEFT_SHRINK_OK, res, "%d");

    struct theft_autoshrink_bit_pool *out = (struct theft_autoshrink_bit_pool *)output;

    /* These bits will construct an LL of {0, 1, 0, 2, 0}:
     * 0b001, 0b00000000,
     * 0b001, 0b00000001,
     * 0b001, 0b00000000,
     * 0b001, 0b00000010,  value had bottom bit masked away
     * 0b001, 0b00000000,
     * 0b000 (end of list) -- 58 bits */
    uint8_t exp_bits[] = { 0x01, 0x48, 0x40, 0x00, 0x22, 0x10, 0x00, 0x00 };
    uint32_t exp_requests[] = { 3, 8,
                                3, 8,
                                3, 8,
                                3, 8,
                                3, 8,
                                3, };
    struct theft_autoshrink_bit_pool exp_pool = {
        .tag = AUTOSHRINK_BIT_POOL_TAG,
        .bits = exp_bits,
        .bits_filled = TEST_POOL_BIT_COUNT,
        .consumed = 5 * (3 + 8) + 3,
        .request_count = sizeof(exp_requests)/sizeof(exp_requests[0]),
        .request_ceil = 999,
        .requests = exp_requests,
    };

    /* Just drop the zeroes off the end */
    ASSERT_EQUAL_T(&exp_pool, out, &bit_pool_info, NULL);

    ll_info.free(out->instance, NULL);
    out->instance = NULL;
    theft_autoshrink_free_bit_pool(t, out);
    theft_free(t);
    PASS();
}

TEST ll_mutate_swap(void) {
    struct theft *t = theft_init(0);

    uint8_t pos_bits = 4; // log2ceil(11)

    struct fake_prng_info prng_info = {
        .pairs = {
            // popcount: 1 change
            { 5, 0x00 /* + 1 */, },

            // swap 4th value
            { pos_bits, 7 },
        },
    };
    struct theft_autoshrink_env env = {
        .tag = AUTOSHRINK_ENV_TAG,
        .user_type_info = ll_info,
        .prng = fake_prng,
        .udata = &prng_info,
        .leave_trailing_zeroes = true,
    };
    theft_autoshrink_model_set_next(&env, ASA_SWAP);

    void *output = NULL;
    enum theft_shrink_res res;
    res = theft_autoshrink_shrink(t, &test_pool, 1, &env, &output);
    ASSERT_EQ_FMT(THEFT_SHRINK_OK, res, "%d");

    struct theft_autoshrink_bit_pool *out = (struct theft_autoshrink_bit_pool *)output;

    /* These bits will construct an LL of {0, 1, 0, 0, 2}:
     * 0b001, 0b00000000,
     * 0b001, 0b00000001,
     * 0b001, 0b00000000,
     * 0b001, 0b00000000, // this and the next value swapped
     * 0b001, 0b00000010,
     * 0b000 (end of list) -- 58 bits */
    uint8_t exp_bits[] = { 0x01, 0x48, 0x40, 0x00, 0x02, 0x90, 0x01, 0x00 };
    uint32_t exp_requests[] = { 3, 8,
                                3, 8,
                                3, 8,
                                3, 8,
                                3, 8,
                                3, };
    struct theft_autoshrink_bit_pool exp_pool = {
        .tag = AUTOSHRINK_BIT_POOL_TAG,
        .bits = exp_bits,
        .bits_filled = TEST_POOL_BIT_COUNT,
        .consumed = 5 * (3 + 8) + 3,
        .request_count = sizeof(exp_requests)/sizeof(exp_requests[0]),
        .request_ceil = 999,
        .requests = exp_requests,
    };

    /* Just drop the zeroes off the end */
    ASSERT_EQUAL_T(&exp_pool, out, &bit_pool_info, NULL);

    ll_info.free(out->instance, NULL);
    out->instance = NULL;
    theft_autoshrink_free_bit_pool(t, out);
    theft_free(t);
    PASS();
}

TEST ll_mutate_sub(void) {
    struct theft *t = theft_init(0);

    uint8_t pos_bits = 4; // log2ceil(11)

    struct fake_prng_info prng_info = {
        .pairs = {
            // popcount: 1 change
            { 5, 0x00 /* + 1 */, },

            // subtract (4 % 3) from 3
            { pos_bits, 7 },
            { 8, 0x04 },
        },
    };
    struct theft_autoshrink_env env = {
        .tag = AUTOSHRINK_ENV_TAG,
        .user_type_info = ll_info,
        .prng = fake_prng,
        .udata = &prng_info,
        .leave_trailing_zeroes = true,
    };
    theft_autoshrink_model_set_next(&env, ASA_SUB);

    void *output = NULL;
    enum theft_shrink_res res;
    res = theft_autoshrink_shrink(t, &test_pool, 1, &env, &output);
    ASSERT_EQ_FMT(THEFT_SHRINK_OK, res, "%d");

    struct theft_autoshrink_bit_pool *out = (struct theft_autoshrink_bit_pool *)output;

    /* These bits will construct an LL of {0, 1, 0, 2, 0}:
     * 0b001, 0b00000000,
     * 0b001, 0b00000001,
     * 0b001, 0b00000000,
     * 0b001, 0b00000010,  value had 1 subtracted from it
     * 0b001, 0b00000000,
     * 0b000 (end of list) -- 58 bits */
    uint8_t exp_bits[] = { 0x01, 0x48, 0x40, 0x00, 0x22, 0x10, 0x00, 0x00 };
    uint32_t exp_requests[] = { 3, 8,
                                3, 8,
                                3, 8,
                                3, 8,
                                3, 8,
                                3, };
    struct theft_autoshrink_bit_pool exp_pool = {
        .tag = AUTOSHRINK_BIT_POOL_TAG,
        .bits = exp_bits,
        .bits_filled = TEST_POOL_BIT_COUNT,
        .consumed = 5 * (3 + 8) + 3,
        .request_count = sizeof(exp_requests)/sizeof(exp_requests[0]),
        .request_ceil = 999,
        .requests = exp_requests,
    };

    /* Just drop the zeroes off the end */
    ASSERT_EQUAL_T(&exp_pool, out, &bit_pool_info, NULL);

    ll_info.free(out->instance, NULL);
    out->instance = NULL;
    theft_autoshrink_free_bit_pool(t, out);
    theft_free(t);
    PASS();
}

TEST ll_mutate_retries_when_change_has_no_effect(void) {
    struct theft *t = theft_init(0);

    uint8_t pos_bits = 4; // log2ceil(11)

    struct fake_prng_info prng_info = {
        .pairs = {
            // popcount: 1 change
            { 5, 0x00 /* + 1 */, },

            // swap 1st value (has no effect)
            { pos_bits, 1 },

            // swap 4th value
            { pos_bits, 7 },
        },
    };
    struct theft_autoshrink_env env = {
        .tag = AUTOSHRINK_ENV_TAG,
        .user_type_info = ll_info,
        .prng = fake_prng,
        .udata = &prng_info,
        .leave_trailing_zeroes = true,
    };
    theft_autoshrink_model_set_next(&env, ASA_SWAP);

    void *output = NULL;
    enum theft_shrink_res res;
    res = theft_autoshrink_shrink(t, &test_pool, 1, &env, &output);
    ASSERT_EQ_FMT(THEFT_SHRINK_OK, res, "%d");

    struct theft_autoshrink_bit_pool *out = (struct theft_autoshrink_bit_pool *)output;

    /* These bits will construct an LL of {0, 1, 0, 0, 2}:
     * 0b001, 0b00000000,
     * 0b001, 0b00000001,
     * 0b001, 0b00000000,
     * 0b001, 0b00000000, // this and the next value swapped
     * 0b001, 0b00000010,
     * 0b000 (end of list) -- 58 bits */
    uint8_t exp_bits[] = { 0x01, 0x48, 0x40, 0x00, 0x02, 0x90, 0x01, 0x00 };
    uint32_t exp_requests[] = { 3, 8,
                                3, 8,
                                3, 8,
                                3, 8,
                                3, 8,
                                3, };
    struct theft_autoshrink_bit_pool exp_pool = {
        .tag = AUTOSHRINK_BIT_POOL_TAG,
        .bits = exp_bits,
        .bits_filled = TEST_POOL_BIT_COUNT,
        .consumed = 5 * (3 + 8) + 3,
        .request_count = sizeof(exp_requests)/sizeof(exp_requests[0]),
        .request_ceil = 999,
        .requests = exp_requests,
    };

    /* Just drop the zeroes off the end */
    ASSERT_EQUAL_T(&exp_pool, out, &bit_pool_info, NULL);

    ll_info.free(out->instance, NULL);
    out->instance = NULL;
    theft_autoshrink_free_bit_pool(t, out);
    theft_free(t);
    PASS();
}

/* Property -- for a randomly generated linked list of numbers,
 * it will not have any duplicated numbers. */
static enum theft_trial_res
prop_no_duplicates(void *arg) {
    struct ll *head = (struct ll *)arg;

    struct ll *cur = head;

    while (cur) {
        assert(head->tag == 'L');
        struct ll *next = cur->next;
        while (next) {
            if (next->value == cur->value) {
                return THEFT_TRIAL_FAIL;
            }
            next = next->next;
        }
        cur = cur->next;
    }

    return THEFT_TRIAL_PASS;
}

/* Property -- for a randomly generated linked list of numbers,
 * the sequence of numbers are not all ascending.
 * The PRNG will generate some runs of ascending numbers;
 * this is to test how well it can automatically shrink them. */
static enum theft_trial_res
prop_not_ascending(void *arg) {
    struct ll *head = (struct ll *)arg;

    struct ll *cur = head;
    uint16_t prev = 0;
    size_t length = 0;

    while (cur) {
        assert(head->tag == 'L');
        length++;
        if (cur->value <= prev) {
            // found a non-ascending value
            return THEFT_TRIAL_PASS;
        }
        prev = cur->value;
        cur = cur->next;
    }

    return (length > 1 ? THEFT_TRIAL_FAIL : THEFT_TRIAL_PASS);
}

/* Property: There won't be any repeated values in the list, with a
 * single non-zero value between them. */
static enum theft_trial_res
prop_no_dupes_with_value_between(void *arg) {
    struct ll *head = (struct ll *)arg;

    struct ll *cur = head;
    uint16_t window[3];
    uint8_t wi = 0;
    while (cur) {
        assert(cur->tag == 'L');
        if (wi == 3) {
            window[0] = window[1];
            window[1] = window[2];
            window[2] = cur->value;
            if ((window[2] == window[0]) &&
                (window[1] != window[0]) &&
                (window[0] != 0)) {
                /* repeated with one value between */
                //printf("FAIL\n");
                return THEFT_TRIAL_FAIL;
            }
        } else {
            window[wi] = cur->value;
            wi++;
        }

        cur = cur->next;
    }

    return THEFT_TRIAL_PASS;
}

/* Property: There won't be any value in the list immediately
 * followed by its square. */
static enum theft_trial_res
prop_no_nonzero_numbers_followed_by_their_square(void *arg) {
    struct ll *head = (struct ll *)arg;

    struct ll *cur = head;

    while (cur) {
        assert(head->tag == 'L');
        struct ll *next = cur->next;
        if (next == NULL) {
            break;
        }
        if (cur->value > 0 && (cur->value * cur->value == next->value)) {
            return THEFT_TRIAL_FAIL;
        }
        cur = next;
    }

    return THEFT_TRIAL_PASS;
}

/* Property: There won't be three values in a row that are
 * [X, X + 1, X + 2]. */
static enum theft_trial_res
prop_no_seq_of_3(void *arg) {
    struct ll *head = (struct ll *)arg;

    struct ll *cur = head;

    while (cur) {
        assert(head->tag == 'L');
        struct ll *next = cur->next;
        if (next && next->next) {
            struct ll *next2 = next->next;
            if ((cur->value + 1 == next->value)
                && (next->value + 1 == next2->value)) {
                return THEFT_TRIAL_FAIL;
            }
        }

        cur = next;
    }

    return THEFT_TRIAL_PASS;
}

struct hook_env {
    struct theft_print_trial_result_env print_env;
    size_t failures;
    bool minimal;
};

static enum theft_hook_trial_pre_res
trial_pre_hook(const struct theft_hook_trial_pre_info *info, void *penv) {
    (void)info;
    struct hook_env *env = (struct hook_env *)penv;
    return env->failures == 5
      ? THEFT_HOOK_TRIAL_PRE_HALT
      : THEFT_HOOK_TRIAL_PRE_CONTINUE;
}

static enum theft_hook_trial_post_res
trial_post_hook(const struct theft_hook_trial_post_info *info, void *penv) {
    struct hook_env *env = (struct hook_env *)penv;
    if (info->result == THEFT_TRIAL_FAIL) {
        env->failures++;
    }

    theft_print_trial_result(&env->print_env, info);

    return THEFT_HOOK_TRIAL_POST_CONTINUE;
}

TEST ll_prop(size_t trials, const char *name, theft_propfun *prop) {
    theft_seed seed = theft_seed_of_time();
    enum theft_run_res res;

    struct hook_env env = { .failures = 0 };

    struct theft_run_config cfg = {
        .name = name,
        .fun = prop,
        .type_info = { &ll_info },
        .hooks = {
            .trial_post = trial_post_hook,
            .trial_pre = trial_pre_hook,
            .run_pre = theft_hook_run_pre_print_info,
            .run_post = theft_hook_run_post_print_info,
            .env = &env,
        },
        .trials = trials,
        .seed = seed,
    };

    res = theft_run(&cfg);
    ASSERT_EQm("should find counter-examples", THEFT_RUN_FAIL, res);
    PASS();
}

static enum theft_trial_res
prop_not_start_with_9(void *arg) {
    uint8_t *ia = (uint8_t *)arg;
    return (ia[0] == 9 ? THEFT_TRIAL_FAIL : THEFT_TRIAL_PASS);
}

TEST ia_prop(const char *name, theft_propfun *prop) {
    theft_seed seed = theft_seed_of_time();
    enum theft_run_res res;

    struct hook_env env = { .failures = 0 };

    struct theft_run_config cfg = {
        .name = name,
        .fun = prop,
        .type_info = { &ia_info },
        .hooks = {
            .trial_post = trial_post_hook,
            .trial_pre = trial_pre_hook,
            .run_pre = theft_hook_run_pre_print_info,
            .run_post = theft_hook_run_post_print_info,
            .env = &env,
        },
        .trials = 50000,
        .seed = seed,
    };

    res = theft_run(&cfg);
    ASSERT_EQm("should find counter-examples", THEFT_RUN_FAIL, res);
    PASS();
}

static enum theft_trial_res
random_bulk_bits_contains_23(void *arg) {
    struct bulk_buffer *bb = (struct bulk_buffer *)arg;

    const size_t limit = bb->size / 8;
    const uint8_t *buf8 = (const uint8_t *)bb->buf;
    for (size_t i = 0; i < limit; i++) {
        if (buf8[i] == 23) {
            return THEFT_TRIAL_FAIL;
        }
    }

    return THEFT_TRIAL_PASS;
}

static enum theft_hook_trial_post_res
bulk_trial_post_hook(const struct theft_hook_trial_post_info *info, void *penv) {
    struct hook_env *env = (struct hook_env *)penv;
    if (info->result == THEFT_TRIAL_FAIL) {
        struct bulk_buffer *bb = info->args[0];
        env->failures++;
        if (bb->size == 8 && bb->buf[0] == 23) {
            env->minimal = true;
        }
    }

    theft_print_trial_result(&env->print_env, info);

    return THEFT_HOOK_TRIAL_POST_CONTINUE;
}

static enum theft_hook_trial_pre_res
bulk_trial_pre_hook(const struct theft_hook_trial_pre_info *info, void *penv) {
    (void)info;
    struct hook_env *env = (struct hook_env *)penv;
    return env->failures == 25
      ? THEFT_HOOK_TRIAL_PRE_HALT
      : THEFT_HOOK_TRIAL_PRE_CONTINUE;
}

TEST bulk_random_bits(void) {
    theft_seed seed = theft_seed_of_time();
    enum theft_run_res res;

    struct hook_env env = { .failures = 0 };

    struct theft_run_config cfg = {
        .name = __func__,
        .fun = random_bulk_bits_contains_23,
        .type_info = { &bb_info },
        .bloom_bits = 20,
        .hooks = {
            .trial_post = bulk_trial_post_hook,
            .trial_pre = bulk_trial_pre_hook,
            .run_pre = theft_hook_run_pre_print_info,
            .run_post = theft_hook_run_post_print_info,
            .env = &env,
        },
        .trials = 1000,
        .seed = seed,
    };

    res = theft_run(&cfg);
    ASSERT_EQm("should find counter-examples", THEFT_RUN_FAIL, res);
    ASSERT(env.minimal);
    PASS();
}

SUITE(autoshrink) {
    // Various tests for single autoshrinking steps, with an injected PRNG
    RUN_TEST(ll_drop_nothing);
    RUN_TEST(ll_drop_nothing_but_do_truncate);
    RUN_TEST(ll_drop_first);
    RUN_TEST(ll_drop_third_and_fourth);
    RUN_TEST(ll_drop_last);
    RUN_TEST(ll_mutate_shift);
    RUN_TEST(ll_mutate_mask);
    RUN_TEST(ll_mutate_swap);
    RUN_TEST(ll_mutate_sub);
    RUN_TEST(ll_mutate_retries_when_change_has_no_effect);

    size_t trials = 50000;
    RUN_TESTp(ll_prop, trials, "no duplicates", prop_no_duplicates);
    RUN_TESTp(ll_prop, trials, "not ascending", prop_not_ascending);
    RUN_TESTp(ll_prop, trials, "no dupes with a non-zero value between",
        prop_no_dupes_with_value_between);
    RUN_TESTp(ll_prop, trials, "no non-zero numbers followed by their square",
        prop_no_nonzero_numbers_followed_by_their_square);

    /* Give this one more trials because occasionally it fails to find
     * counterexamples */
    RUN_TESTp(ll_prop, 2*trials, "no sequence of three numbers",
        prop_no_seq_of_3);

    RUN_TESTp(ia_prop, "not starting with 9", prop_not_start_with_9);

    RUN_TEST(bulk_random_bits);
}
