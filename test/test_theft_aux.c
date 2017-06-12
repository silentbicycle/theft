#include "test_theft.h"
#include "theft_aux.h"

struct a_squared_env {
    struct theft_print_trial_result_env print_env;
    bool found;
};

/* Property: a^2 <= 12345 */
static enum theft_trial_res
prop_a_squared_lte_fixed(void *arg) {
    int8_t a = *(int8_t *)arg;
    uint16_t b = 12345;
    const size_t aa = a * a;

    return (aa <= b)
      ? THEFT_TRIAL_PASS
      : THEFT_TRIAL_FAIL;
}

static enum theft_hook_trial_post_res
fixed_expected_failure_trial_post(const struct theft_hook_trial_post_info *info,
                            void *penv) {
    struct a_squared_env *env = (struct a_squared_env *)penv;
    int8_t a = *(int8_t *)info->args[0];

    if (greatest_get_verbosity() > 0) {
        printf("### trial_post: res %s for [arg: %d (%p)]\n\n",
            theft_trial_res_str(info->result),
            a, (void *)info->args[0]);
    }

    /* Set found if the min failure is found. */
    if (info->result == THEFT_TRIAL_FAIL) {
        int8_t a = *(int8_t *)info->args[0];
        //printf("FAILURE: %d\n", a);
        if (a == 112 || a == -112) {  // min failure: 112 * 112 > 12345
            env->found = true;
        }
    }

    return THEFT_HOOK_TRIAL_POST_CONTINUE;
}

static enum theft_hook_shrink_trial_post_res
fixed_log_shrink_trial_post(const struct theft_hook_shrink_trial_post_info *info,
        void *env) {
    (void)env;

    int8_t a = *(int8_t *)info->args[0];

    if (greatest_get_verbosity() > 0) {
        printf("### shrink_trial_post: res %s for arg %u [arg: %d (%p)]\n\n",
            theft_trial_res_str(info->result), info->arg_index,
            a, (void *)info->args[0]);
    }
    return THEFT_HOOK_SHRINK_TRIAL_POST_CONTINUE;
}

TEST a_squared_lte_fixed(void) {
    theft_seed seed = theft_seed_of_time();

    struct a_squared_env env;
    memset(&env, 0x00, sizeof(env));

    struct theft_run_config cfg = {
        .name = __func__,
        .fun = prop_a_squared_lte_fixed,
        .type_info = {
            theft_get_builtin_type_info(THEFT_BUILTIN_int8_t),
        },
        .bloom_bits = 20,
        .seed = seed,
        .trials = 500,
        .hooks = {
            .run_pre = theft_hook_run_pre_print_info,
            .run_post = theft_hook_run_post_print_info,
            .trial_post = fixed_expected_failure_trial_post,
            .shrink_trial_post = fixed_log_shrink_trial_post,
            .env = &env,
        },
    };

    ASSERT_EQ_FMTm("should find counter-examples",
        THEFT_RUN_FAIL, theft_run(&cfg), "%d");
    ASSERTm("Should shrink to a minimal case", env.found);
    PASS();
}

static enum theft_trial_res
prop_a_squared_lt_b(void *arg_a, void *arg_b) {
    int8_t a = *(int8_t *)arg_a;
    uint16_t b = *(uint16_t *)arg_b;
    const size_t aa = a * a;

    if (0) {
        fprintf(stdout, "\n$$ checking (%d^2) < %u ? %d (a^2 = %zd)\n",
            a, b, aa < b, aa);
    }

    return (aa <= b)
      ? THEFT_TRIAL_PASS
      : THEFT_TRIAL_FAIL;
}

static enum theft_hook_trial_post_res
expected_failure_trial_post(const struct theft_hook_trial_post_info *info,
                            void *penv) {
    struct a_squared_env *env = (struct a_squared_env *)penv;
    int8_t a = *(int8_t *)info->args[0];
    uint16_t b = *(uint16_t *)info->args[1];

    if (greatest_get_verbosity() > 0) {
        printf("### trial_post: res %s for [args: %d (%p), %u (%p)]\n\n",
            theft_trial_res_str(info->result),
            a, (void *)info->args[0],
            b, (void *)info->args[1]);
    }

    if (info->result == THEFT_TRIAL_FAIL) {
        int8_t a = *(int8_t *)info->args[0];
        uint16_t b = *(uint16_t *)info->args[1];
        if (a == 1 && b == 0) {
            env->found = true;
        }
    }

    return THEFT_HOOK_TRIAL_POST_CONTINUE;
}

static enum theft_hook_shrink_trial_post_res
log_shrink_trial_post(const struct theft_hook_shrink_trial_post_info *info,
        void *env) {
    (void)env;

    int8_t a = *(int8_t *)info->args[0];
    uint16_t b = *(uint16_t *)info->args[1];

    if (greatest_get_verbosity() > 0) {
        printf("### shrink_trial_post: res %s for arg %u [args: %d (%p), %u (%p)]\n\n",
            theft_trial_res_str(info->result), info->arg_index,
            a, (void *)info->args[0],
            b, (void *)info->args[1]);
    }
    return THEFT_HOOK_SHRINK_TRIAL_POST_CONTINUE;
}

TEST a_squared_lt_b(void) {
    theft_seed seed = theft_seed_of_time();

    struct a_squared_env env;
    memset(&env, 0x00, sizeof(env));

    struct theft_run_config cfg = {
        .name = __func__,
        .fun = prop_a_squared_lt_b,
        .type_info = {
            theft_get_builtin_type_info(THEFT_BUILTIN_int8_t),
            theft_get_builtin_type_info(THEFT_BUILTIN_uint16_t),
        },
        .bloom_bits = 20,
        .seed = seed,
        .trials = 500,
        .hooks = {
            .run_pre = theft_hook_run_pre_print_info,
            .run_post = theft_hook_run_post_print_info,
            .trial_post = expected_failure_trial_post,
            .shrink_trial_post = log_shrink_trial_post,
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
    RUN_TEST(a_squared_lte_fixed);
    RUN_TEST(a_squared_lt_b);
}
