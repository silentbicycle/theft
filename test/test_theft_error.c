#include "test_theft.h"

enum behavior {
    BEH_NONE,
    BEH_SKIP_ALL,
    BEH_ERROR_ALL,
    BEH_SKIP_DURING_AUTOSHRINK,
    BEH_FAIL_DURING_AUTOSHRINK,
    BEH_SHRINK_ERROR,
};

struct err_env {
    uint8_t tag;
    enum behavior b;
    bool shrinking;
};

static enum theft_trial_res prop_bits_gt_0(struct theft *t, void *arg1) {
    uint8_t *x = (uint8_t *)arg1;
    (void)t;
    return (*x > 0 ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL);
}

static enum theft_alloc_res
bits_alloc(struct theft *t, void *penv, void **output) {
    assert(penv);
    struct err_env *env = (struct err_env *)penv;
    assert(env->tag == 'e');

    if (env->b == BEH_SKIP_ALL) {
        return THEFT_ALLOC_SKIP;
    } else if (env->b == BEH_ERROR_ALL) {
        return THEFT_ALLOC_ERROR;
    }

    if (env->shrinking) {
        env->shrinking = false;
        if (env->b == BEH_SKIP_DURING_AUTOSHRINK) {
            return THEFT_ALLOC_SKIP;
        } else if (env->b == BEH_FAIL_DURING_AUTOSHRINK) {
            return THEFT_ALLOC_ERROR;
        }
    }

    uint8_t *res = calloc(1, sizeof(*res));
    if (res == NULL) {
        return THEFT_ALLOC_ERROR;
    }
    *res = theft_random_bits(t, 6);
    *output = res;
    return THEFT_ALLOC_OK;
}

TEST alloc_returns_skip(void) {
    struct err_env env = {
        .tag = 'e',
        .b = BEH_SKIP_ALL,
    };

    static struct theft_type_info type_info = {
        .alloc = bits_alloc,
        .free = theft_generic_free_cb,
        .autoshrink_config = {
            .enable = true,
        },
    };
    type_info.env = &env;

    struct theft_run_config cfg = {
        .prop1 = prop_bits_gt_0,
        .type_info = { &type_info },
        .trials = 10,
        .hooks = {
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);

    ASSERT_EQ_FMT(THEFT_RUN_SKIP, res, "%d");
    PASS();
}

static enum theft_trial_res prop_should_never_run(struct theft *t,
        void *arg1, void *arg2) {
    (void)t; (void)arg1; (void)arg2;
    return THEFT_TRIAL_ERROR;
}

/* Check that arguments which have already been generated
 * aren't double-freed. */
TEST second_alloc_returns_skip(void) {
    struct err_env env = {
        .tag = 'e',
        .b = BEH_SKIP_ALL,
    };

    static struct theft_type_info type_info = {
        .alloc = bits_alloc,
        .free = theft_generic_free_cb,
        .autoshrink_config = {
            .enable = true,
        },
    };
    type_info.env = &env;

    struct theft_run_config cfg = {
        .prop2 = prop_should_never_run,
        .type_info = {
            theft_get_builtin_type_info(THEFT_BUILTIN_uint16_t),
            &type_info },
        .trials = 10,
        .hooks = {
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);

    ASSERT_EQ_FMT(THEFT_RUN_SKIP, res, "%d");
    PASS();
}

TEST alloc_returns_error(void) {
    struct err_env env = {
        .tag = 'e',
        .b = BEH_ERROR_ALL,
    };

    static struct theft_type_info type_info = {
        .alloc = bits_alloc,
        .free = theft_generic_free_cb,
        .autoshrink_config = {
            .enable = true,
        },
    };
    type_info.env = &env;

    struct theft_run_config cfg = {
        .prop1 = prop_bits_gt_0,
        .type_info = { &type_info },
        .trials = 10,
        .hooks = {
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);

    ASSERT_EQ_FMT(THEFT_RUN_ERROR, res, "%d");
    PASS();
}

TEST second_alloc_returns_error(void) {
    struct err_env env = {
        .tag = 'e',
        .b = BEH_ERROR_ALL,
    };

    static struct theft_type_info type_info = {
        .alloc = bits_alloc,
        .free = theft_generic_free_cb,
        .autoshrink_config = {
            .enable = true,
        },
    };
    type_info.env = &env;

    struct theft_run_config cfg = {
        .prop2 = prop_should_never_run,
        .type_info = {
            theft_get_builtin_type_info(THEFT_BUILTIN_uint16_t),
            &type_info },
        .trials = 10,
        .hooks = {
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);

    ASSERT_EQ_FMT(THEFT_RUN_ERROR, res, "%d");
    PASS();
}

static enum theft_hook_shrink_pre_res
shrink_pre_set_shrinking(const struct theft_hook_shrink_pre_info *info,
        void *penv) {
    (void)info;
    struct err_env *env = (struct err_env *)penv;
    assert(env->tag == 'e');
    env->shrinking = true;
    return THEFT_HOOK_SHRINK_PRE_CONTINUE;
}

TEST alloc_returns_skip_during_autoshrink(void) {
    struct err_env env = {
        .tag = 'e',
        .b = BEH_SKIP_DURING_AUTOSHRINK,
    };

    static struct theft_type_info type_info = {
        .alloc = bits_alloc,
        .free = theft_generic_free_cb,
        .autoshrink_config = {
            .enable = true,
        },
    };
    type_info.env = &env;

    struct theft_run_config cfg = {
        .prop1 = prop_bits_gt_0,
        .type_info = { &type_info },
        .trials = 1000,
        .hooks = {
            .shrink_pre = shrink_pre_set_shrinking,
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);

    ASSERT_EQ_FMT(THEFT_RUN_FAIL, res, "%d");
    PASS();
}

TEST alloc_returns_error_during_autoshrink(void) {
    struct err_env env = {
        .tag = 'e',
        .b = BEH_FAIL_DURING_AUTOSHRINK,
    };

    static struct theft_type_info type_info = {
        .alloc = bits_alloc,
        .free = theft_generic_free_cb,
        .autoshrink_config = {
            .enable = true,
        },
    };
    type_info.env = &env;

    struct theft_run_config cfg = {
        .prop1 = prop_bits_gt_0,
        .type_info = { &type_info },
        .trials = 1000,
        .hooks = {
            .shrink_pre = shrink_pre_set_shrinking,
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);

    ASSERT_EQ_FMT(THEFT_RUN_ERROR, res, "%d");
    PASS();
}

static enum theft_shrink_res
bits_shrink(struct theft *t, const void *instance, uint32_t tactic,
        void *penv, void **output) {
    (void)t;
    assert(penv);
    struct err_env *env = (struct err_env *)penv;
    if (env->b == BEH_SHRINK_ERROR) {
        return THEFT_SHRINK_ERROR;
    }

    if (tactic == 2) {
        return THEFT_SHRINK_NO_MORE_TACTICS;
    }

    const uint8_t *bits = (const uint8_t *)instance;
    uint8_t *res = calloc(1, sizeof(*res));
    if (res == NULL) {
        return THEFT_SHRINK_ERROR;
    }
    *res = (tactic == 0 ? (*bits / 2) : (*bits - 1));
    *output = res;
    return THEFT_SHRINK_OK;
}

TEST error_from_both_autoshrink_and_shrink_cb(void) {
    struct err_env env = {
        .tag = 'e',
        .b = BEH_NONE,
    };

    static struct theft_type_info type_info = {
        .alloc = bits_alloc,
        .free = theft_generic_free_cb,
        .shrink = bits_shrink,
        .autoshrink_config = {
            .enable = true,
        },
    };
    type_info.env = &env;

    struct theft_run_config cfg = {
        .prop1 = prop_bits_gt_0,
        .type_info = { &type_info },
        .hooks = {
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);
    ASSERT_ENUM_EQm("defining shrink and autoshrink should error",
        THEFT_RUN_ERROR_BAD_ARGS, res, theft_run_res_str);
    PASS();
}

TEST shrinking_error(void) {
    struct err_env env = {
        .tag = 'e',
        .b = BEH_SHRINK_ERROR,
    };

    static struct theft_type_info type_info = {
        .alloc = bits_alloc,
        .free = theft_generic_free_cb,
        .shrink = bits_shrink,
    };
    type_info.env = &env;

    struct theft_run_config cfg = {
        .prop1 = prop_bits_gt_0,
        .type_info = { &type_info },
        .trials = 10000,
        .hooks = {
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);

    ASSERT_EQ_FMT(THEFT_RUN_ERROR, res, "%d");
    PASS();
}

static enum theft_hook_run_pre_res
hook_run_pre_error(const struct theft_hook_run_pre_info *info,
        void *penv) {
    struct err_env *env = (struct err_env *)penv;
    assert(env->tag == 'e');
    (void)info;
    return THEFT_HOOK_RUN_PRE_ERROR;
}

TEST run_pre_hook_error(void) {
    struct err_env env = {
        .tag = 'e',
        .b = BEH_NONE,
    };

    static struct theft_type_info type_info = {
        .alloc = bits_alloc,
        .free = theft_generic_free_cb,
        .autoshrink_config = {
            .enable = true,
        },
    };
    type_info.env = &env;

    struct theft_run_config cfg = {
        .prop1 = prop_bits_gt_0,
        .type_info = { &type_info },
        .trials = 10000,
        .hooks = {
            .run_pre = hook_run_pre_error,
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);
    ASSERT_EQ_FMT(THEFT_RUN_ERROR, res, "%d");
    PASS();
}

static enum theft_hook_run_post_res
hook_run_post_error(const struct theft_hook_run_post_info *info,
        void *penv) {
    struct err_env *env = (struct err_env *)penv;
    assert(env->tag == 'e');
    (void)info;
    return THEFT_HOOK_RUN_POST_ERROR;
}

TEST run_post_hook_error(void) {
    struct err_env env = {
        .tag = 'e',
        .b = BEH_NONE,
    };

    static struct theft_type_info type_info = {
        .alloc = bits_alloc,
        .free = theft_generic_free_cb,
        .autoshrink_config = {
            .enable = true,
        },
    };
    type_info.env = &env;

    struct theft_run_config cfg = {
        .prop1 = prop_bits_gt_0,
        .type_info = { &type_info },
        .trials = 10000,
        .hooks = {
            .run_post = hook_run_post_error,
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);
    ASSERT_EQ_FMT(THEFT_RUN_ERROR, res, "%d");
    PASS();
}

static enum theft_hook_trial_pre_res
hook_trial_pre_error(const struct theft_hook_trial_pre_info *info,
        void *penv) {
    struct err_env *env = (struct err_env *)penv;
    assert(env->tag == 'e');
    (void)info;
    return THEFT_HOOK_TRIAL_PRE_ERROR;
}

TEST trial_pre_hook_error(void) {
    struct err_env env = {
        .tag = 'e',
        .b = BEH_NONE,
    };

    static struct theft_type_info type_info = {
        .alloc = bits_alloc,
        .free = theft_generic_free_cb,
        .autoshrink_config = {
            .enable = true,
        },
    };
    type_info.env = &env;

    struct theft_run_config cfg = {
        .prop1 = prop_bits_gt_0,
        .type_info = { &type_info },
        .trials = 10000,
        .hooks = {
            .trial_pre = hook_trial_pre_error,
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);
    ASSERT_EQ_FMT(THEFT_RUN_ERROR, res, "%d");
    PASS();
}

static enum theft_hook_trial_post_res
hook_trial_post_error(const struct theft_hook_trial_post_info *info,
        void *penv) {
    struct err_env *env = (struct err_env *)penv;
    assert(env->tag == 'e');
    (void)info;
    return THEFT_HOOK_TRIAL_POST_ERROR;
}

TEST trial_post_hook_error(void) {
    struct err_env env = {
        .tag = 'e',
        .b = BEH_NONE,
    };

    static struct theft_type_info type_info = {
        .alloc = bits_alloc,
        .free = theft_generic_free_cb,
        .autoshrink_config = {
            .enable = true,
        },
    };
    type_info.env = &env;

    struct theft_run_config cfg = {
        .prop1 = prop_bits_gt_0,
        .type_info = { &type_info },
        .trials = 10000,
        .hooks = {
            .trial_post = hook_trial_post_error,
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);
    ASSERT_EQ_FMT(THEFT_RUN_ERROR, res, "%d");
    PASS();
}

static enum theft_hook_shrink_pre_res
hook_shrink_pre_error(const struct theft_hook_shrink_pre_info *info,
    void *penv) {
    struct err_env *env = (struct err_env *)penv;
    assert(env->tag == 'e');
    (void)info;
    return THEFT_HOOK_SHRINK_PRE_ERROR;
}

TEST shrink_pre_hook_error(void) {
    struct err_env env = {
        .tag = 'e',
        .b = BEH_NONE,
    };

    static struct theft_type_info type_info = {
        .alloc = bits_alloc,
        .free = theft_generic_free_cb,
        .autoshrink_config = {
            .enable = true,
        },
    };
    type_info.env = &env;

    struct theft_run_config cfg = {
        .prop1 = prop_bits_gt_0,
        .type_info = { &type_info },
        .trials = 10000,
        .hooks = {
            .shrink_pre = hook_shrink_pre_error,
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);
    ASSERT_EQ_FMT(THEFT_RUN_ERROR, res, "%d");
    PASS();
}


static enum theft_hook_shrink_post_res
hook_shrink_post_error(const struct theft_hook_shrink_post_info *info,
    void *penv) {
    struct err_env *env = (struct err_env *)penv;
    assert(env->tag == 'e');
    (void)info;
    return THEFT_HOOK_SHRINK_POST_ERROR;
}

TEST shrink_post_hook_error(void) {
    struct err_env env = {
        .tag = 'e',
        .b = BEH_NONE,
    };

    static struct theft_type_info type_info = {
        .alloc = bits_alloc,
        .free = theft_generic_free_cb,
        .autoshrink_config = {
            .enable = true,
        },
    };
    type_info.env = &env;

    struct theft_run_config cfg = {
        .prop1 = prop_bits_gt_0,
        .type_info = { &type_info },
        .trials = 10000,
        .hooks = {
            .shrink_post = hook_shrink_post_error,
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);
    ASSERT_EQ_FMT(THEFT_RUN_ERROR, res, "%d");
    PASS();
}

static enum theft_hook_shrink_trial_post_res
hook_shrink_trial_post_error(const struct theft_hook_shrink_trial_post_info *info,
    void *penv) {
    struct err_env *env = (struct err_env *)penv;
    assert(env->tag == 'e');
    (void)info;
    return THEFT_HOOK_SHRINK_TRIAL_POST_ERROR;
}

TEST shrink_trial_post_hook_error(void) {
    struct err_env env = {
        .tag = 'e',
        .b = BEH_NONE,
    };

    static struct theft_type_info type_info = {
        .alloc = bits_alloc,
        .free = theft_generic_free_cb,
        .autoshrink_config = {
            .enable = true,
        },
    };
    type_info.env = &env;

    struct theft_run_config cfg = {
        .prop1 = prop_bits_gt_0,
        .type_info = { &type_info },
        .trials = 10000,
        .hooks = {
            .shrink_trial_post = hook_shrink_trial_post_error,
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);
    ASSERT_EQ_FMT(THEFT_RUN_ERROR, res, "%d");
    PASS();
}

static enum theft_trial_res prop_always_skip(struct theft *t, void *arg1) {
    uint8_t *x = (uint8_t *)arg1;
    (void)t;
    (void)x;
    return THEFT_TRIAL_SKIP;
}

TEST trial_skip(void) {
    struct err_env env = {
        .tag = 'e',
        .b = BEH_NONE,
    };

    static struct theft_type_info type_info = {
        .alloc = bits_alloc,
        .free = theft_generic_free_cb,
        .autoshrink_config = {
            .enable = true,
        },
    };
    type_info.env = &env;

    struct theft_run_config cfg = {
        .prop1 = prop_always_skip,
        .type_info = { &type_info },
        .trials = 10,
        .hooks = {
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);

    ASSERT_EQ_FMT(THEFT_RUN_SKIP, res, "%d");
    PASS();
}

static enum theft_trial_res prop_always_error(struct theft *t, void *arg1) {
    uint8_t *x = (uint8_t *)arg1;
    (void)t;
    (void)x;
    return THEFT_TRIAL_ERROR;
}

TEST trial_error(void) {
    struct err_env env = {
        .tag = 'e',
        .b = BEH_NONE,
    };

    static struct theft_type_info type_info = {
        .alloc = bits_alloc,
        .free = theft_generic_free_cb,
        .autoshrink_config = {
            .enable = true,
        },
    };
    type_info.env = &env;

    struct theft_run_config cfg = {
        .prop1 = prop_always_error,
        .type_info = { &type_info },
        .trials = 10,
        .hooks = {
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);

    ASSERT_EQ_FMT(THEFT_RUN_ERROR, res, "%d");
    PASS();
}

static bool trial_error_during_autoshrink_flag = false;

static enum theft_trial_res prop_error_if_autoshrinking(struct theft *t, void *arg1) {
    uint8_t *x = (uint8_t *)arg1;
    (void)t;
    if (trial_error_during_autoshrink_flag) {
        return THEFT_TRIAL_ERROR;
    }
    return prop_bits_gt_0(t, x);
}

static enum theft_hook_shrink_pre_res
shrink_pre_set_shrinking_global_flag(const struct theft_hook_shrink_pre_info *info,
        void *penv) {
    struct err_env *env = (struct err_env *)penv;
    assert(env->tag == 'e');
    (void)info;
    trial_error_during_autoshrink_flag = true;
    return THEFT_HOOK_SHRINK_PRE_CONTINUE;
}

TEST trial_error_during_autoshrink(void) {
    struct err_env env = {
        .tag = 'e',
        .b = BEH_NONE,
    };

    static struct theft_type_info type_info = {
        .alloc = bits_alloc,
        .free = theft_generic_free_cb,
        .autoshrink_config = {
            .enable = true,
        },
    };
    type_info.env = &env;

    struct theft_run_config cfg = {
        .prop1 = prop_error_if_autoshrinking,
        .type_info = { &type_info },
        .trials = 1000,
        .hooks = {
            .env = (void *)&env,
            .shrink_pre = shrink_pre_set_shrinking_global_flag,
        },
    };

    enum theft_run_res res = theft_run(&cfg);

    ASSERT_EQ_FMT(THEFT_RUN_ERROR, res, "%d");
    PASS();
}

static enum theft_hook_counterexample_res
hook_counterexample_error(const struct theft_hook_counterexample_info *info,
        void *penv) {
    struct err_env *env = (struct err_env *)penv;
    assert(env->tag == 'e');
    (void)info;
    return THEFT_HOOK_COUNTEREXAMPLE_ERROR;
}

TEST counterexample_error(void) {
    struct err_env env = {
        .tag = 'e',
        .b = BEH_NONE,
    };

    static struct theft_type_info type_info = {
        .alloc = bits_alloc,
        .free = theft_generic_free_cb,
        .autoshrink_config = {
            .enable = true,
        },
    };
    type_info.env = &env;

    struct theft_run_config cfg = {
        .prop1 = prop_bits_gt_0,
        .type_info = { &type_info },
        .trials = 10000,
        .hooks = {
            .counterexample = hook_counterexample_error,
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);
    ASSERT_EQ_FMT(THEFT_RUN_ERROR, res, "%d");
    PASS();
}

static enum theft_trial_res prop_ignore_input_fail_then_pass(struct theft *t, void *arg1) {
    uint8_t *x = (uint8_t *)arg1;
    (void)t;
    (void)x;
    static size_t runs = 0;
    runs++;
    return (runs == 1 ? THEFT_TRIAL_FAIL : THEFT_TRIAL_PASS);
}

static enum theft_hook_trial_post_res
trial_post_repeat_once(const struct theft_hook_trial_post_info *info,
        void *penv) {
    struct err_env *env = (struct err_env *)penv;
    assert(env->tag == 'e');
    (void)info;
    return THEFT_HOOK_TRIAL_POST_REPEAT_ONCE;
}

TEST fail_but_pass_when_rerun(void) {
    struct err_env env = {
        .tag = 'e',
        .b = BEH_NONE,
    };

    static struct theft_type_info type_info = {
        .alloc = bits_alloc,
        .free = theft_generic_free_cb,
        .shrink = bits_shrink,
    };
    type_info.env = &env;

    struct theft_run_config cfg = {
        .prop1 = prop_ignore_input_fail_then_pass,
        .type_info = { &type_info },
        .trials = 1,
        .hooks = {
            .trial_post = trial_post_repeat_once,
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);
    ASSERT_EQ_FMT(THEFT_RUN_ERROR, res, "%d");
    PASS();
}


/* Various tests related to exercising error handling. */
SUITE(error) {
    RUN_TEST(alloc_returns_skip);
    RUN_TEST(alloc_returns_error);
    RUN_TEST(alloc_returns_skip_during_autoshrink);
    RUN_TEST(alloc_returns_error_during_autoshrink);
    RUN_TEST(second_alloc_returns_skip);
    RUN_TEST(second_alloc_returns_error);
    RUN_TEST(shrinking_error);
    RUN_TEST(error_from_both_autoshrink_and_shrink_cb);
    RUN_TEST(run_pre_hook_error);
    RUN_TEST(run_post_hook_error);
    RUN_TEST(trial_pre_hook_error);
    RUN_TEST(trial_post_hook_error);
    RUN_TEST(shrink_pre_hook_error);
    RUN_TEST(shrink_post_hook_error);
    RUN_TEST(shrink_trial_post_hook_error);
    RUN_TEST(trial_skip);
    RUN_TEST(trial_error);
    RUN_TEST(trial_error_during_autoshrink);
    RUN_TEST(counterexample_error);
    RUN_TEST(fail_but_pass_when_rerun);
}
