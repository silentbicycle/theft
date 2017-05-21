#include "test_theft.h"
#include "theft_aux.h"

struct a_squared_lte_b_env {
    struct theft_print_trial_result_env print_env;
};

static enum theft_trial_res
prop_a_squared_lte_b(void *arg_a, void *arg_b) {
    int8_t a = *(int8_t *)arg_a;
    uint16_t b = *(uint16_t *)arg_b;

    if (0) {
        fprintf(stdout, "\n$$ checking (%d * %d) < %u => %d ? %d\n",
        a, a, b, a * a, a * a <= b);
    }

    return ((a * a) <= b)
      ? THEFT_TRIAL_PASS
      : THEFT_TRIAL_FAIL;
}

TEST a_squared_lte_b(void) {
    theft_seed seed = theft_seed_of_time();

    struct a_squared_lte_b_env env;
    memset(&env, 0x00, sizeof(env));

    struct theft_run_config cfg = {
        .name = __func__,
        .fun = prop_a_squared_lte_b,
        .type_info = {
            theft_get_builtin_type_info(THEFT_BUILTIN_int8_t),
            theft_get_builtin_type_info(THEFT_BUILTIN_uint16_t),
        },
        .bloom_bits = 20,
        .seed = seed,
        .hooks = {
            .run_pre = theft_hook_run_pre_print_info,
            .run_post = theft_hook_run_post_print_info,
            .env = &env,
        },
    };

    ASSERT_EQ_FMTm("should find counter-examples",
        THEFT_RUN_FAIL, theft_run(&cfg), "%d");
    PASS();
}

SUITE(aux) {
    // builtins
    RUN_TEST(a_squared_lte_b);
}
