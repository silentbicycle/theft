#include "test_theft.h"
#include "theft_aux.h"

struct a_squared_lte_b_env {
    struct theft_print_trial_result_env print_env;
    bool found;
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

static enum theft_hook_trial_post_res
expected_failure_trial_post(const struct theft_hook_trial_post_info *info,
                            void *penv) {
    struct a_squared_lte_b_env *env = (struct a_squared_lte_b_env *)penv;
    if (info->result == THEFT_TRIAL_FAIL) {
        int8_t a = *(int8_t *)info->args[0];
        uint16_t b = *(uint16_t *)info->args[1];
        printf("FAILURE: %d, %u\n", a, b);
        if (a == 0 && b == 1) {
            env->found = true;
        }
    }

    return THEFT_HOOK_TRIAL_POST_CONTINUE;
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
            .trial_post = expected_failure_trial_post,
            .env = &env,
        },
    };

    ASSERT_EQ_FMTm("should find counter-examples",
        THEFT_RUN_FAIL, theft_run(&cfg), "%d");
    ASSERTm("Should shrink to a minimal case", env.found);
    PASS();
}

SUITE(aux) {
    // builtins
    RUN_TEST(a_squared_lte_b);
}
