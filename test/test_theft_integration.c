#include "test_theft.h"

#include "theft_rng.h"

#include <sys/time.h>
#include <assert.h>
#include <inttypes.h>
#include <signal.h>

#include <sys/resource.h>

#define COUNT(X) (sizeof(X)/sizeof(X[0]))

static enum theft_alloc_res
uint_alloc(struct theft *t, void *env, void **output) {
    uint32_t *n = malloc(sizeof(uint32_t));
    if (n == NULL) { return THEFT_ALLOC_ERROR; }
    *n = (uint32_t)(theft_random_bits(t, 8*sizeof(uint32_t)));
    (void)t; (void)env;
    *output = n;
    return THEFT_ALLOC_OK;
}

static void uint_free(void *p, void *env) {
    (void)env;
    free(p);
}

static void uint_print(FILE *f, const void *p, void *env) {
    (void)env;
    fprintf(f, "%u", *(uint32_t *)p);
}

static struct theft_type_info uint_type_info = {
    .alloc = uint_alloc,
    .free = uint_free,
    .print = uint_print,
};

static enum theft_trial_res
is_pos(struct theft *t, void *arg1) {
    uint32_t *n = (uint32_t *)arg1;
    /* Ignoring the argument and returning true, because *n is positive
     * by definition -- checking gets a tautological comparison warning. */
    (void)t;
    (void)n;
    return THEFT_TRIAL_PASS;
}

TEST generated_unsigned_ints_are_positive(void) {
    enum theft_run_res res;

    /* The configuration struct can be passed in as an argument literal,
     * though you have to cast it. */
    res = theft_run(&(struct theft_run_config){
            .name = "generated_unsigned_ints_are_positive",
            .prop1 = is_pos,
            .type_info = { &uint_type_info },
        });
    ASSERT_EQm("generated_unsigned_ints_are_positive",
        THEFT_RUN_PASS, res);
    PASS();
}

/* Linked list of ints. */
typedef struct list {
    int32_t v;
    struct list *next;
} list;

static void list_free(void *instance, void *env) {
    list *l = (list *)instance;

    while (l) {
        list *nl = l->next;
        free(l);
        l = nl;
    }
    (void)env;
}

static void list_unpack_seed(theft_hash seed,
        int32_t *lower, uint32_t *upper) {
    *lower = (int32_t)(seed & 0xFFFFFFFF);
    *upper = (uint32_t)((seed >> 32) & 0xFFFFFFFF);

}

static enum theft_alloc_res
list_alloc(struct theft *t, void *env, void **output) {
    (void)env;
    list *l = NULL;             /* empty */

    int32_t lower = 0;
    uint32_t upper = 0;
    int len = 0;

    theft_seed seed = theft_random_bits(t, 64);
    list_unpack_seed(seed, &lower, &upper);

    while (upper >= (uint32_t)(0x40000000 | (1 << len))) {
        if (len < 31) {
            len++;
        } else {
            break;
        }
        list *nl = malloc(sizeof(list));
        if (nl) {
            /* Limit to 0 ~ 1023 so uniqueness test will have failures. */
            nl->v = lower & (1024 - 1);
            /* nl->v = lower; */
            nl->next = l;
            l = nl;
        } else {
            list_free(l, NULL);
        }

        seed = theft_random_bits(t, 64);
        list_unpack_seed(seed, &lower, &upper);
    }

    *output = l;
    return THEFT_ALLOC_OK;
}

static theft_hash list_hash(const void *instance, void *env) {
    list *l = (list *)instance;
    struct theft_hasher h;
    theft_hash_init(&h);

    /* printf("\nhashing list %p...", l); */

    while (l) {
        /* printf("%d, ", l->v); */
        theft_hash_sink(&h, (uint8_t *)&l->v, 4);
        l = l->next;
    }
    (void)env;
    theft_hash res = theft_hash_done(&h);
    /* printf(" => %llu\n", res); */
    return res;
}

static list *copy_list(list *l) {
    list *res = NULL;
    list *cur = NULL;

    while (l) {
        list *nl = malloc(sizeof(*nl));
        if (nl == NULL) {
            list_free(res, NULL);
            return NULL;
        }
        nl->v = l->v;
        if (res == NULL) {
            res = nl;           /* store head for return */
            cur = nl;
        } else {
            cur->next = nl;     /* append at tail */
            cur = nl;
        }
        nl->next = NULL;
        l = l->next;
    }

    return res;
}

static int list_length(list *l) {
    int len = 0;
    while (l) {
        len++;
        l = l->next;
    }
    return len;
}

static enum theft_shrink_res
split_list_copy(list *l, bool first_half, list **output) {
    int len = list_length(l);
    if (len < 2) { return THEFT_SHRINK_DEAD_END; }
    list *nl = copy_list(l);
    if (nl == NULL) { return THEFT_SHRINK_ERROR; }
    list *t = nl;
    for (int i = 0; i < len/2 - 1; i++) { t = t->next; }

    list *tail = t->next;
    t->next = NULL;
    if (first_half) {
        list_free(tail, NULL);
        *output = nl;
    } else {
        list_free(nl, NULL);
        *output = tail;
    }
    return THEFT_SHRINK_OK;
}

static enum theft_shrink_res
list_shrink(struct theft *t, const void *instance, uint32_t tactic,
        void *env, void **output) {
    (void)t;
    list *l = (list *)instance;
    if (l == NULL) { return THEFT_SHRINK_NO_MORE_TACTICS; }

    /* When reducing, it's faster to have the tactics ordered by how
     * much they simplify the instance, if possible. In this case, we
     * first try discarding either half of the list, then dividing the
     * whole list by 2, before operations that only impact one element
     * of the list. */

    if (tactic == 0) {          /* first half */
        return split_list_copy(l, true, (list **)output);
    } else if (tactic == 1) {   /* second half */
        return split_list_copy(l, false, (list **)output);
    } else if (tactic == 2) {      /* div whole list by 2 */
        bool nonzero = false;
        for (list *link = l; link; link = link->next) {
            if (link->v > 0) { nonzero = true; break; }
        }

        if (nonzero) {
            list *nl = copy_list(l);
            if (nl == NULL) { return THEFT_SHRINK_ERROR; }

            for (list *link = nl; link; link = link->next) {
                link->v /= 2;
            }
            *output = nl;
            return THEFT_SHRINK_OK;
        } else {
            return THEFT_SHRINK_DEAD_END;
        }
    } else if (tactic == 3) {      /* drop head */
        if (l->next == NULL) { return THEFT_SHRINK_DEAD_END; }
        list *nl = copy_list(l->next);
        if (nl == NULL) { return THEFT_SHRINK_ERROR; }
        list *nnl = nl->next;
        nl->next = NULL;
        list_free(nl, NULL);
        *output = nnl;
        return THEFT_SHRINK_OK;
    } else if (tactic == 4) {      /* drop tail */
        if (l->next == NULL) { return THEFT_SHRINK_DEAD_END; }

        list *nl = copy_list(l);
        if (nl == NULL) { return THEFT_SHRINK_ERROR; }
        list *prev = nl;
        list *tl = nl;

        while (tl->next) {
            prev = tl;
            tl = tl->next;
        }
        prev->next = NULL;
        list_free(tl, NULL);
        *output = nl;
        return THEFT_SHRINK_OK;
    } else {
        (void)instance;
        (void)tactic;
        (void)env;
        return THEFT_SHRINK_NO_MORE_TACTICS;
    }
}

static void list_print(FILE *f, const void *instance, void *env) {
    fprintf(f, "(");
    list *l = (list *)instance;
    while (l) {
        fprintf(f, "%s%d", l == instance ? "" : " ", l->v);
        l = l->next;
    }
    fprintf(f, ")");
    (void)env;
}

static struct theft_type_info list_info = {
    .alloc = list_alloc,
    .free = list_free,
    .hash = list_hash,
    .shrink = list_shrink,
    .print = list_print,
};

static enum theft_trial_res prop_gen_cons(struct theft *t, void *arg1) {
    list *l = (list *)arg1;
    (void)t;
    list *nl = malloc(sizeof(list));
    if (nl == NULL) { return THEFT_TRIAL_ERROR; }
    nl->v = 0;
    nl->next = l;

    enum theft_trial_res res;
    if (list_length(nl) == list_length(l) + 1) {
        res = THEFT_TRIAL_PASS;
    } else {
        res = THEFT_TRIAL_FAIL;
    }
    free(nl);
    return res;
}

TEST generated_int_list_with_cons_is_longer(void) {
    enum theft_run_res res;
    struct theft_run_config cfg = {
        .name = __func__,
        .prop1 = prop_gen_cons,
        .type_info = { &list_info },
    };
    res = theft_run(&cfg);

    ASSERT_EQ(THEFT_RUN_PASS, res);
    PASS();
}

static enum theft_trial_res
prop_gen_list_unique(struct theft *t, void *arg1) {
    list *l = (list *)arg1;
    (void)t;
    /* For each link in the list, check that there are none later that
     * have the same value. (This should fail, as we're not constraining
     * against it in list generation.) */

    while (l) {
        for (list *nl = l->next; nl; nl = nl->next) {
            if (nl->v == l->v) { return THEFT_TRIAL_FAIL; }
        }
        l = l->next;
    }

    return THEFT_TRIAL_PASS;
}

struct test_env {
    int dots;
    size_t fail;
};

static enum theft_hook_trial_post_res
gildnrv_trial_post_hook(const struct theft_hook_trial_post_info *info, void *penv) {
    struct test_env *env = (struct test_env *)penv;
    if (info->result == THEFT_TRIAL_FAIL) {
        printf("f");
        env->fail++;
    } else if ((info->trial_id % 100) == 0) {
        printf(".");
    }
    return THEFT_HOOK_TRIAL_POST_CONTINUE;
}

TEST generated_int_list_does_not_repeat_values(void) {
    /* This test is expected to fail, with meaningful counter-examples. */
    struct test_env env = { .fail = false };

    struct theft_run_config cfg = {
        .name = __func__,
        .prop1 = prop_gen_list_unique,
        .type_info = { &list_info },
        .hooks = {
            .trial_post = gildnrv_trial_post_hook,
            .env = &env,
        },
        .trials = 1000,
        .seed = 12345,
    };

    enum theft_run_res res;
    res = theft_run(&cfg);
    ASSERT_EQ_FMTm("should find counter-examples", THEFT_RUN_FAIL, res, "%d");
    ASSERT(env.fail > 0);
    PASS();
}

static enum theft_trial_res
prop_gen_list_unique_pair(struct theft *t, void *arg1, void *arg2) {
    list *a = (list *)arg1;
    list *b = (list *)arg2;

    (void)t;
    if (list_length(a) == list_length(b)) {
        list *la;
        list *lb = b;

        for (la = a; la && lb; la = la->next, lb = lb->next) {
            if (la->v != lb->v) { break; }
        }

        /* If they match all the way to the end */
        if (la == NULL && lb == NULL) {
            return THEFT_TRIAL_FAIL;
        }
    }

    return THEFT_TRIAL_PASS;
}

static enum theft_hook_trial_post_res
trial_post_hook_cb(const struct theft_hook_trial_post_info *info, void *env) {
    struct test_env *e = (struct test_env *)env;

    if ((info->trial_id % 100) == 0) {
        printf(".");
        e->dots++;
    }
    if (e->dots == 72) {
        e->dots = 0;
        printf("\n");
    }

    if (info->result == THEFT_TRIAL_FAIL) {
        e->fail = true;
    }

    return THEFT_HOOK_TRIAL_POST_CONTINUE;
}

TEST two_generated_lists_do_not_match(void) {
    /* This test is expected to fail, with meaningful counter-examples. */
    struct test_env env;
    memset(&env, 0, sizeof(env));

    struct timeval tv;
    if (-1 == gettimeofday(&tv, NULL)) { FAIL(); }

    enum theft_run_res res;
    struct theft_run_config cfg = {
        .name = __func__,
        .prop2 = prop_gen_list_unique_pair,
        .type_info = { &list_info, &list_info },
        .trials = 10000,
        .hooks = {
            .trial_post = trial_post_hook_cb,
            .env = &env,
        },
        .seed = (theft_seed)(tv.tv_sec ^ tv.tv_usec)
    };
    res = theft_run(&cfg);
    ASSERT_EQm("should find counter-examples", THEFT_RUN_FAIL, res);
    ASSERT(env.fail);
    PASS();
}

typedef struct {
    int dots;
    int checked;
    size_t fail;
} always_seed_env;

static enum theft_hook_trial_post_res
always_seeds_trial_post(const struct theft_hook_trial_post_info *info, void *venv) {
    always_seed_env *env = (always_seed_env *)venv;
    if ((info->trial_id % 100) == 0) {
        printf(".");
    }

    theft_seed seed = info->trial_seed;
    /* Must run 'always' seeds */
    if (seed == 0x600d5eed) { env->checked |= 0x01; }
    if (seed == 0xabad5eed) { env->checked |= 0x02; }

    /* Must also otherwise start from specified seed */
    if (seed == 0x600dd06) { env->checked |= 0x04; }

    if (info->result == THEFT_TRIAL_FAIL) {
        env->fail = true;
    }
    return THEFT_HOOK_TRIAL_POST_CONTINUE;
}

/* Or, the optional always_seed fields could be wrapped in a macro... */
#define ALWAYS_SEEDS(A)                                               \
      .always_seed_count = COUNT(A),                                  \
      .always_seeds = A

TEST always_seeds_must_be_run(void) {
    /* This test is expected to fail, with meaningful counter-examples. */
    static theft_hash always_seeds[] = {
        0x600d5eed, 0xabad5eed,
    };

    always_seed_env env;
    memset(&env, 0, sizeof(env));

    enum theft_run_res res;
    struct theft_run_config cfg = {
        .name = __func__,
        .prop1 = prop_gen_list_unique,
        .type_info = { &list_info },
        .trials = 1000,
        .seed = 0x600dd06,
        ALWAYS_SEEDS(always_seeds),
        .hooks = {
            .trial_post = always_seeds_trial_post,
            .env = &env,
        },
    };
    res = theft_run(&cfg);
    ASSERT_EQm("should find counter-examples", THEFT_RUN_FAIL, res);
    ASSERT(env.fail > 0);
    if (0x03 != (env.checked & 0x03)) { FAILm("'always' seeds were not run"); }
    if (0x04 != (env.checked & 0x04)) { FAILm("starting seed was not run"); }
    PASS();
}

#define EXPECTED_SEED 0x15a600d64b175eedLL

static enum theft_alloc_res
seed_alloc(struct theft *t, void *env, void **output) {
    uint64_t *res = malloc(sizeof(*res));
    if (res == NULL) { return THEFT_ALLOC_ERROR; }
    (void)env;
    (void)t;
    *res = theft_random_bits(t, 64);
    *output = res;
    return THEFT_ALLOC_OK;
}

static void seed_free(void *instance, void *env) {
    (void)env;
    free(instance);
}

static struct theft_type_info seed_info = {
    .alloc = seed_alloc,
    .free = seed_free,
};

static uint64_t expected_value = 0;

static enum theft_trial_res
prop_expected_seed_is_used(struct theft *t, void *arg0) {
    theft_seed *s = (theft_seed *)arg0;
    (void)t;
    if (*s == expected_value) {
        return THEFT_TRIAL_PASS;
    } else {
        return THEFT_TRIAL_FAIL;
    }
}

TEST expected_seed_should_be_used_first(void) {
    struct theft_rng *rng = theft_rng_init(EXPECTED_SEED);
    expected_value = theft_rng_random(rng);
    theft_rng_free(rng);

    struct theft_run_config cfg = {
        .name = __func__,
        .prop1 = prop_expected_seed_is_used,
        .type_info = { &seed_info },
        .trials = 1,
        .seed = EXPECTED_SEED,
    };

    enum theft_run_res res = theft_run(&cfg);
    ASSERT_EQ(THEFT_RUN_PASS, res);
    PASS();
}

static enum theft_trial_res
prop_bool_tautology(struct theft *t, void *arg1) {
    bool *bp = (bool *)arg1;
    (void)t;
    bool b = *bp;
    if (b || !b) {    // tautology to force shrinking
        return THEFT_TRIAL_FAIL;
    } else {
        return THEFT_TRIAL_PASS;
    }
}

static enum theft_alloc_res
bool_alloc(struct theft *t, void *env, void **output) {
    bool *bp = malloc(sizeof(*bp));
    if (bp == NULL) { return THEFT_ALLOC_ERROR; }
    *bp = theft_random_bits(t, 1) ? true : false;
    (void)env;
    (void)t;
    *output = bp;
    return THEFT_ALLOC_OK;
}

static void bool_free(void *instance, void *env) {
    (void)env;
    free(instance);
}

static theft_hash bool_hash(const void *instance, void *env) {
    bool *bp = (bool *)instance;
    bool b = *bp;
    (void)env;
    return (theft_hash)(b ? 1 : 0);
}

static struct theft_type_info bool_info = {
    .alloc = bool_alloc,
    .free = bool_free,
    .hash = bool_hash,
};

static enum theft_hook_run_post_res
save_report_run_post(const struct theft_hook_run_post_info *info, void *env) {
    struct theft_run_report *report = (struct theft_run_report *)env;
    memcpy(report, &info->report, sizeof(*report));
    return THEFT_HOOK_RUN_POST_CONTINUE;
}

TEST overconstrained_state_spaces_should_be_detected(void) {
    struct theft_run_report report = {
        .pass = 0,
    };

    struct theft_run_config cfg = {
        .prop1 = prop_bool_tautology,
        .type_info = { &bool_info },
        .trials = 100,
        .hooks = {
            .run_post = save_report_run_post,
            .env = (void *)&report,
        },
    };

    enum theft_run_res res = theft_run(&cfg);
    ASSERT_EQ(THEFT_RUN_FAIL, res);
    ASSERT_EQ(2, report.fail);
    ASSERT_EQ(98, report.dup);
    PASS();
}

static enum theft_alloc_res
never_run_alloc(struct theft *t, void *env, void **output) {
    (void)t;
    (void)env;
    (void)output;
    *output = NULL;
    return THEFT_ALLOC_OK;
}

static struct theft_type_info never_run_info = {
    .alloc = never_run_alloc,
};

static enum theft_hook_gen_args_pre_res
error_in_gen_args_pre(const struct theft_hook_gen_args_pre_info *info, void *env) {
    theft_seed *seed = (theft_seed *)env;
    *seed = info->trial_seed;
    return THEFT_HOOK_GEN_ARGS_PRE_ERROR;
}

static enum theft_trial_res
should_never_run(struct theft *t, void *arg1) {
    void *x = (void *)arg1;
    (void)t;
    (void)x;
    assert(false);
    return THEFT_TRIAL_ERROR;
}

TEST save_seed_and_error_before_generating_args(void) {
    theft_seed seed = 0;

    struct theft_run_config cfg = {
        .prop1 = should_never_run,
        .type_info = { &never_run_info },
        .hooks = {
            .gen_args_pre = error_in_gen_args_pre,
            .env = (void *)&seed,
        },
        .seed = 0xf005ba1L,
    };

    enum theft_run_res res = theft_run(&cfg);

    ASSERT_EQ(THEFT_RUN_ERROR, res);
    ASSERT_EQ_FMT((uint64_t)0xf005ba1L, seed, "%" PRIx64);

    PASS();
}

static enum theft_trial_res
always_pass(struct theft *t, void *arg1) {
    void *x = (void *)arg1;
    (void)t;
    (void)x;
    return THEFT_TRIAL_PASS;
}

static enum theft_hook_trial_pre_res
halt_before_third_trial_pre(const struct theft_hook_trial_pre_info *info,
    void *env) {
    (void)env;
    if (info->trial_id == 2) {
        return THEFT_HOOK_TRIAL_PRE_HALT;
    }
    return THEFT_HOOK_TRIAL_PRE_CONTINUE;
}

static enum theft_hook_run_post_res
halt_before_third_run_post(const struct theft_hook_run_post_info *info, void *env) {
    struct theft_run_report *report = (struct theft_run_report *)env;
    memcpy(report, &info->report, sizeof(*report));
    return THEFT_HOOK_RUN_POST_CONTINUE;
}

TEST gen_pre_halt(void) {
    struct theft_run_report report = { .pass = 0 };

    struct theft_run_config cfg = {
        .prop1 = always_pass,
        .type_info = { &uint_type_info },
        .hooks = {
            .trial_pre = halt_before_third_trial_pre,
            .run_post = halt_before_third_run_post,
            .env = (void *)&report,
        },
    };

    enum theft_run_res res = theft_run(&cfg);

    ASSERT_EQ(THEFT_RUN_PASS, res);
    ASSERT_EQ_FMT((size_t)2, report.pass, "%zd");
    ASSERT_EQ_FMT((size_t)0, report.fail, "%zd");
    ASSERT_EQ_FMT((size_t)0, report.skip, "%zd");
    ASSERT_EQ_FMT((size_t)0, report.dup, "%zd");

    PASS();
}


static enum theft_trial_res
prop_uint_is_lte_12345(struct theft *t, void *arg1) {
    uint32_t *arg = (uint32_t *)arg1;
    (void)t;
    return *arg <= 12345 ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

static enum theft_shrink_res
uint_shrink(struct theft *t, const void *instance, uint32_t tactic,
        void *env, void **output) {
    (void)t;
    (void)env;
    const uint32_t *pnum = (const uint32_t *)instance;
    uint32_t *res = malloc(sizeof(*pnum));
    if (res == NULL) {
        return THEFT_SHRINK_ERROR;
    }

    *res = *pnum;
    if (tactic == 0) {
        (*res) -= (*res / 4);
        *output = res;
        return THEFT_SHRINK_OK;
    } else if (tactic == 1) {
        (*res)--;
        *output = res;
        return THEFT_SHRINK_OK;
    } else {
        free(res);
        return THEFT_SHRINK_NO_MORE_TACTICS;
    }
}

static enum theft_alloc_res
shrink_test_uint_alloc(struct theft *t, void *env, void **output) {
    uint32_t *n = malloc(sizeof(uint32_t));
    if (n == NULL) { return THEFT_ALLOC_ERROR; }
    uint32_t value = (uint32_t)theft_random_bits(t, 32);
    /* Make sure the value is large enough that we can test
     * cancelling shrinking early. */
    if (value < 100000) {
        value += 100000;
    }
    *n = value;
    (void)t; (void)env;
    *output = n;
    return THEFT_ALLOC_OK;
}


static struct theft_type_info shrink_test_uint_type_info = {
    .alloc = shrink_test_uint_alloc,
    .free = uint_free,
    .print = uint_print,
    .shrink = uint_shrink,
};

struct shrink_test_env {
    size_t shrinks;
    size_t fail;
    uint32_t local_minimum;
    size_t reruns;
};

static enum theft_hook_shrink_pre_res
halt_after_third_shrink_shrink_pre(const struct theft_hook_shrink_pre_info *info,
    void *venv) {
    struct shrink_test_env *env = (struct shrink_test_env *)venv;
    printf("shrink_pre: trial %zd, seed %" PRIx64 "\n",
        info->trial_id, info->trial_seed);
    printf("    shrink_count %zd, succ %zd, fail %zd\n",
        info->shrink_count, info->successful_shrinks,
        info->failed_shrinks);
    uint32_t *pnum = (uint32_t *)info->arg;
    printf("    BEFORE: %u\n", *pnum);
    if (info->successful_shrinks == 3) {
        env->shrinks = 3;
        return THEFT_HOOK_SHRINK_PRE_HALT;
    } else if (info->successful_shrinks > 3) {
        env->fail = true;
    }
    return THEFT_HOOK_SHRINK_PRE_CONTINUE;
}

static enum theft_hook_shrink_post_res
halt_after_third_shrink_shrink_post(const struct theft_hook_shrink_post_info *info,
    void *env) {
    (void)env;
    printf("shrink_post: trial %zd, seed %" PRIx64 "\n",
        info->trial_id, info->trial_seed);
    printf("    shrink_count %zd, succ %zd, fail %zd\n",
        info->shrink_count, info->successful_shrinks,
        info->failed_shrinks);
    uint32_t *pnum = (uint32_t *)info->arg;
    printf("    AFTER: %u, done? %d\n",
        *pnum, info->state == THEFT_SHRINK_POST_DONE_SHRINKING);
    return THEFT_HOOK_SHRINK_POST_CONTINUE;
}

TEST only_shrink_three_times(void) {
    struct shrink_test_env env = { .shrinks = 0 };

    struct theft_run_config cfg = {
        .prop1 = prop_uint_is_lte_12345,
        .type_info = { &shrink_test_uint_type_info },
        .trials = 1,
        .hooks = {
            .shrink_pre = halt_after_third_shrink_shrink_pre,
            .shrink_post = halt_after_third_shrink_shrink_post,
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);

    ASSERT_EQ(THEFT_RUN_FAIL, res);
    ASSERT(!env.fail);
    ASSERT_EQ_FMT((size_t)3, env.shrinks, "%zd");
    PASS();
}

static enum theft_hook_shrink_trial_post_res
shrink_all_the_way_shrink_trial_post(const struct theft_hook_shrink_trial_post_info *info,
    void *venv) {
    struct shrink_test_env *env = (struct shrink_test_env *)venv;
    uint32_t *pnum = (uint32_t *)info->args[0];
    if (*pnum == 12346 && env->reruns < 3) {
        env->reruns++;
        return THEFT_HOOK_SHRINK_TRIAL_POST_REPEAT;
    }
    return THEFT_HOOK_SHRINK_TRIAL_POST_CONTINUE;
}

static enum theft_hook_shrink_post_res
shrink_all_the_way_shrink_post(const struct theft_hook_shrink_post_info *info,
    void *venv) {
    struct shrink_test_env *env = (struct shrink_test_env *)venv;
    if (info->state == THEFT_SHRINK_POST_DONE_SHRINKING) {
        uint32_t *pnum = (uint32_t *)info->arg;
        env->local_minimum = *pnum;
        printf("Saving local minimum %u after %zd shrinks (succ %zd, fail %zd) -- %p\n",
            env->local_minimum,
            info->shrink_count,
            info->successful_shrinks,
            info->failed_shrinks,
            (void *)info->arg);
    }
    return THEFT_HOOK_SHRINK_POST_CONTINUE;
}

static enum theft_hook_trial_post_res
shrink_all_the_way_trial_post(const struct theft_hook_trial_post_info *info, void *venv) {
    struct shrink_test_env *env = (struct shrink_test_env *)venv;
    uint32_t *pnum = (uint32_t *)info->args[0];
    if (*pnum == 12346 && env->reruns < 33) {
        env->reruns += 10;
        return THEFT_HOOK_TRIAL_POST_REPEAT;
    }
    return THEFT_HOOK_TRIAL_POST_CONTINUE;
}

TEST save_local_minimum_and_re_run(void) {
    struct shrink_test_env env = { .shrinks = 0 };

    struct theft_run_config cfg = {
        .prop1 = prop_uint_is_lte_12345,
        .type_info = { &shrink_test_uint_type_info },
        .hooks = {
            .trial_post = shrink_all_the_way_trial_post,
            .shrink_trial_post = shrink_all_the_way_shrink_trial_post,
            .shrink_post = shrink_all_the_way_shrink_post,
            .env = (void *)&env,
        },
        .trials = 1,
    };

    enum theft_run_res res = theft_run(&cfg);

    ASSERT_EQ(THEFT_RUN_FAIL, res);
    ASSERT(!env.fail);
    ASSERT_EQ_FMTm("three trial-post and three shrink-post hook runs",
        (size_t)33, env.reruns, "%zd");
    ASSERT_EQ_FMT(12346U, env.local_minimum, "%" PRIu32);
    PASS();
}

struct repeat_once_env {
    uint8_t local_minimum_runs;
    bool fail;
};

static enum theft_hook_trial_pre_res
repeat_once_trial_pre(const struct theft_hook_trial_pre_info *info,
    void *venv) {
    (void)info;
    struct repeat_once_env *env = (struct repeat_once_env *)venv;
    /* Only run one failing trial. */
    if (env->fail) {
        return THEFT_HOOK_TRIAL_PRE_HALT;
    }
    return THEFT_HOOK_TRIAL_PRE_CONTINUE;
}

static enum theft_hook_trial_post_res
repeat_once_trial_post(const struct theft_hook_trial_post_info *info,
    void *venv) {
    struct repeat_once_env *env = (struct repeat_once_env *)venv;
    if (info->result == THEFT_TRIAL_FAIL) {
        env->fail = true;
        env->local_minimum_runs++;
        if (env->local_minimum_runs > 2) {
            /* Shouldn't get here, but don't repeat forever */
            return THEFT_HOOK_TRIAL_POST_ERROR;
        } else {
            return THEFT_HOOK_TRIAL_POST_REPEAT_ONCE;
        }
    }
    return THEFT_HOOK_TRIAL_POST_CONTINUE;
}

TEST repeat_local_minimum_once(void) {
    enum theft_run_res res;

    struct repeat_once_env env = { .fail = false };

    struct theft_run_config cfg = {
        .name = __func__,
        .prop1 = prop_gen_list_unique,
        .type_info = { &list_info },
        .hooks = {
            .trial_pre = repeat_once_trial_pre,
            .trial_post = repeat_once_trial_post,
            .env = &env,
        },
        .trials = 1000,
        .seed = 12345,
    };

    res = theft_run(&cfg);
    ASSERT_EQm("should find counter-examples", THEFT_RUN_FAIL, res);
    ASSERT(env.fail);
    ASSERT_EQ_FMT(2, env.local_minimum_runs, "%u");
    PASS();
}

struct repeat_shrink_once_env {
    void *last_arg;
    uint8_t shrink_repeats;
    bool fail;
};

static enum theft_hook_trial_pre_res
repeat_first_successful_shrink_then_halt_trial_pre(const struct theft_hook_trial_pre_info *info,
    void *venv) {
    (void)info;
    struct repeat_shrink_once_env *env = (struct repeat_shrink_once_env *)venv;
    if (env->last_arg != NULL) {
        return THEFT_HOOK_TRIAL_PRE_HALT;
    }
    return THEFT_HOOK_TRIAL_PRE_CONTINUE;
}

static enum theft_hook_trial_post_res
repeat_first_successful_shrink_then_halt_trial_post(const struct theft_hook_trial_post_info *info,
    void *venv) {
    struct repeat_shrink_once_env *env = (struct repeat_shrink_once_env *)venv;
    if (info->result == THEFT_TRIAL_FAIL) {
        env->fail = true;
        if (env->last_arg == info->args[0]) {
            assert(info->args[0]);
            env->shrink_repeats++;
        } else {
            env->last_arg = info->args[0];
        }

        if (env->shrink_repeats > 1) {
            /* Shouldn't get here, but don't repeat forever */
            return THEFT_HOOK_TRIAL_POST_ERROR;
        } else {
            return THEFT_HOOK_TRIAL_POST_REPEAT_ONCE;
        }
    }

    return THEFT_HOOK_TRIAL_POST_CONTINUE;
}

static enum theft_hook_shrink_pre_res
repeat_first_successful_shrink_then_halt_shrink_pre(const struct theft_hook_shrink_pre_info *info,
    void *venv) {
    (void)info;
    struct repeat_shrink_once_env *env = (struct repeat_shrink_once_env *)venv;
    if (env->last_arg != NULL) {
        return THEFT_HOOK_SHRINK_PRE_HALT;
    }
    return THEFT_HOOK_SHRINK_PRE_CONTINUE;
}

TEST repeat_first_successful_shrink_once_then_halt(void) {
    enum theft_run_res res;

    struct repeat_shrink_once_env env = { .fail = false };

    struct theft_run_config cfg = {
        .name = __func__,
        .prop1 = prop_gen_list_unique,
        .type_info = { &list_info },
        .hooks = {
            .trial_pre = repeat_first_successful_shrink_then_halt_trial_pre,
            .trial_post = repeat_first_successful_shrink_then_halt_trial_post,
            .shrink_pre = repeat_first_successful_shrink_then_halt_shrink_pre,
            .env = &env,
        },
        .trials = 1000,
        .seed = 12345,
    };

    res = theft_run(&cfg);
    ASSERT_EQm("should find counter-examples", THEFT_RUN_FAIL, res);
    ASSERT(env.fail);
    ASSERT_EQ_FMT(1, env.shrink_repeats, "%u");
    PASS();
}

struct crash_env {
    bool minimum;
};

static enum theft_hook_trial_post_res
found_10(const struct theft_hook_trial_post_info *info,
        void *venv) {
    struct crash_env *env = (struct crash_env *)venv;

    if (info->result == THEFT_TRIAL_FAIL) {
        const uint16_t v = *(const uint16_t *)info->args[0];
        if (v == 10) {
            env->minimum = true;
        }
    }

    return THEFT_HOOK_TRIAL_POST_CONTINUE;
}

static enum theft_hook_trial_pre_res
halt_if_found_10(const struct theft_hook_trial_pre_info *info,
        void *venv) {
    (void)info;
    struct crash_env *env = (struct crash_env *)venv;
    return env->minimum
      ? THEFT_HOOK_TRIAL_PRE_HALT
      : THEFT_HOOK_TRIAL_PRE_CONTINUE;
}

#include <poll.h>
static enum theft_trial_res
prop_crash_with_int_gte_10(struct theft *t, void *arg1) {
    uint16_t *v = (uint16_t *)arg1;
    (void)t;
    if (*v >= 10) {
        abort();
    }
    return THEFT_TRIAL_PASS;
}

TEST shrink_crash(void) {
    enum theft_run_res res;

    struct crash_env env = { .minimum = false };

    struct theft_run_config cfg = {
        .name = __func__,
        .prop1 = prop_crash_with_int_gte_10,
        .type_info = { theft_get_builtin_type_info(THEFT_BUILTIN_uint16_t) },
        .trials = 1000,
        .fork = {
            .enable = true,
            .timeout = 10000,
        },
        .hooks = {
            .trial_pre = halt_if_found_10,
            .trial_post = found_10,
            .env = &env,
        },
    };

    res = theft_run(&cfg);
    ASSERT_EQm("should find counter-examples", THEFT_RUN_FAIL, res);
    ASSERT(env.minimum);
    PASS();
}

static enum theft_trial_res
prop_just_abort(struct theft *t, void *arg1) {
    uint64_t *v = (uint64_t *)arg1;
    (void)t;
    (void)v;
    abort();
    return THEFT_TRIAL_PASS;
}

/* This calls a property test that just aborts immediately,
 * but has a much larger state space due to the uint64_t
 * argument -- this will lead to lots and lots of forking,
 * and should eventually run into fork failures with EAGAIN
 * due to RLIMIT_NPROC. This should exercise theft's
 * exponential back-off and cleaning up of terminated
 * child processes. */
TEST shrink_abort_immediately_to_stress_forking__slow(void) {
    enum theft_run_res res;

    struct crash_env env = { .minimum = false };

    struct theft_run_config cfg = {
        .name = __func__,
        .prop1 = prop_just_abort,
        .type_info = { theft_get_builtin_type_info(THEFT_BUILTIN_uint64_t) },
        .trials = 1000,
        .fork = {
            .enable = true,
            .timeout = 10000,
        },
        .hooks = {
            .trial_pre = halt_if_found_10,
            .trial_post = found_10,
            .env = &env,
        },
    };

    res = theft_run(&cfg);
    ASSERT_EQm("should fail, but recover from temporary fork failures",
        THEFT_RUN_FAIL, res);

    PASS();
}

static enum theft_trial_res
prop_infinite_loop_with_int_gte_10(struct theft *t, void *arg1) {
    uint16_t *v = (uint16_t *)arg1;
    (void)t;
    if (*v >= 10) {
        for (;;) {}
    }
    return THEFT_TRIAL_PASS;
}

TEST shrink_infinite_loop(void) {
    enum theft_run_res res;

    struct crash_env env = { .minimum = false };

    struct theft_run_config cfg = {
        .name = __func__,
        .prop1 = prop_infinite_loop_with_int_gte_10,
        .type_info = { theft_get_builtin_type_info(THEFT_BUILTIN_uint16_t) },
        .trials = 1000,
        .fork = {
            .enable = true,
            .timeout = 100,
        },
        .hooks = {
            .trial_pre = halt_if_found_10,
            .trial_post = found_10,
            .env = &env,
        },
    };

    res = theft_run(&cfg);
    ASSERT_EQm("should find counter-examples", THEFT_RUN_FAIL, res);
    ASSERT(env.minimum);
    PASS();
}

static volatile bool sigusr1_handled_flag = false;

static void sigusr1_handler(int sig) {
    if (sig == SIGUSR1) {
        sigusr1_handled_flag = true;
    }
}

static enum theft_trial_res
prop_wait_for_SIGUSR1(struct theft *t, void *arg1) {
    bool *v = (bool *)arg1;
    (void)t;
    (void)v;
    struct sigaction action = {
        .sa_handler = sigusr1_handler,
    };
    struct sigaction old_action;
    if (-1 == sigaction(SIGUSR1, &action, &old_action)) {
        return THEFT_TRIAL_ERROR;
    }

    sigusr1_handled_flag = false;

    for (;;) {
        poll(NULL, 0, 10);
        if (sigusr1_handled_flag) {
            return THEFT_TRIAL_PASS;
        }
    }

    return THEFT_TRIAL_FAIL;
}

TEST shrink_and_SIGUSR1_on_timeout(void) {
    enum theft_run_res res;

    struct theft_run_config cfg = {
        .name = __func__,
        .prop1 = prop_wait_for_SIGUSR1,
        .type_info = { theft_get_builtin_type_info(THEFT_BUILTIN_bool) },
        .trials = 1,
        .fork = {
            .enable = true,
            .timeout = 10,
            .signal = SIGUSR1,
        },
    };

    res = theft_run(&cfg);
    ASSERT_EQm("should pass due to exit(EXIT_SUCCESS)", THEFT_RUN_PASS, res);
    PASS();
}

static enum theft_trial_res
prop_infinite_loop(struct theft *t, void *arg1) {
    bool *v = (bool *)arg1;
    (void)t;
    (void)v;

    struct sigaction action = {
        .sa_handler = sigusr1_handler,
    };
    struct sigaction old_action;
    if (-1 == sigaction(SIGUSR1, &action, &old_action)) {
        return THEFT_TRIAL_ERROR;
    }

    for (;;) {
        (void)poll(NULL, 0, 1);
    }

    return THEFT_TRIAL_ERROR;
}

/* Send the worker process a SIGUSR1 after a 10 msec timeout.
 * Since it ignores the SIGUSR1, then make sure we send it
 * a SIGKILL instead, so the test still terminates. */
TEST shrink_and_SIGUSR1_on_timeout_then_SIGKILL(void) {
    enum theft_run_res res;

    struct theft_run_config cfg = {
        .name = __func__,
        .prop1 = prop_infinite_loop,
        .type_info = { theft_get_builtin_type_info(THEFT_BUILTIN_bool) },
        .trials = 1,
        .fork = {
            .enable = true,
            .timeout = 10,
            .signal = SIGUSR1,
        },
    };

    res = theft_run(&cfg);
    ASSERT_EQm("should fail: worker didn't exit after timeout signal",
        THEFT_RUN_FAIL, res);
    PASS();
}

static bool printed_verbose_msg = false;

static enum theft_trial_res
prop_even_and_not_between_100_and_5000(struct theft *t, void *arg1) {
    uint64_t *pv = (uint64_t *)arg1;
    bool *verbose = theft_hook_get_env(t);
    if (verbose == NULL) { return THEFT_TRIAL_ERROR; }

    uint64_t v = *pv;
    if (v & 0x01) {
        if (*verbose) {
            fprintf(stdout, "Failing: %" PRIu64 " is odd\n", v);
            printed_verbose_msg = true;
        }
        return THEFT_TRIAL_FAIL;
    }

    if (v >= 100 && v <= 5000) {
        if (*verbose) {
            fprintf(stdout, "Failing: %" PRIu64 " is between 100 and 5000\n", v);
            printed_verbose_msg = true;
        }
        return THEFT_TRIAL_FAIL;
    }

    return THEFT_TRIAL_PASS;
}

struct verbose_test_env {
    char tag;
    bool verbose;
    struct theft_print_trial_result_env print_env;
};

/* Re-run each failure once, with the verbose flag set. */
enum theft_hook_trial_post_res
trial_post_repeat_with_verbose_set(const struct theft_hook_trial_post_info *info,
    void *env) {
    struct verbose_test_env *test_env = (struct verbose_test_env *)env;
    assert(test_env->tag == 'V');
    test_env->verbose = false;

    if (info->result == THEFT_TRIAL_FAIL) {
        test_env->verbose = !info->repeat; /* verbose next time */
        return THEFT_HOOK_TRIAL_POST_REPEAT_ONCE;
    }

    return theft_hook_trial_post_print_result(info, &test_env->print_env);
}

TEST repeat_with_verbose_set_after_shrinking(void) {
    enum theft_run_res res;

    struct verbose_test_env env = {
        .tag = 'V',
    };

    struct theft_run_config cfg = {
        .name = __func__,
        .prop1 = prop_even_and_not_between_100_and_5000,
        .type_info = { theft_get_builtin_type_info(THEFT_BUILTIN_uint64_t) },
        .trials = 100,
        .hooks = {
            .trial_pre = theft_hook_first_fail_halt,
            .trial_post = trial_post_repeat_with_verbose_set,
            .env = &env,
        },
    };

    res = theft_run(&cfg);
    ASSERT_ENUM_EQm("should find counterexamples",
        THEFT_RUN_FAIL, res, theft_run_res_str);
    ASSERTm("should repeat once with verbose set on failure",
        printed_verbose_msg);
    PASS();
}

struct fork_post_env {
    bool hook_flag;
    uint16_t value;
};

static enum theft_hook_fork_post_res
fork_post_set_flag(const struct theft_hook_fork_post_info *info, void *env) {
    (void)info;
    struct fork_post_env *hook_env = (struct fork_post_env *)env;
    hook_env->value = *(uint16_t *)info->args[0];
    return THEFT_HOOK_FORK_POST_CONTINUE;
}

static enum theft_trial_res
prop_ignore_input_return_fork_hook(struct theft *t, void *arg1) {
    uint16_t value = *(uint16_t *)arg1;

    struct fork_post_env *hook_env = theft_hook_get_env(t);
    if (hook_env == NULL) { return THEFT_TRIAL_ERROR; }

    /* Check that the value is the same in the fork_post hook
     * and when the property runs -- this is mainly a guard
     * against incorrect autoshrinker boxing/unboxing. */
    if (hook_env->value == value) {
        hook_env->hook_flag = true;
    }

    return hook_env->hook_flag
      ? THEFT_TRIAL_PASS
      : THEFT_TRIAL_FAIL;
}

TEST forking_hook(void) {
    enum theft_run_res res;

    struct fork_post_env env = {
        .value = 0,
    };

    struct theft_run_config cfg = {
        .name = __func__,
        .prop1 = prop_ignore_input_return_fork_hook,
        .type_info = { theft_get_builtin_type_info(THEFT_BUILTIN_uint16_t) },
        .trials = 1,
        .hooks = {
            .fork_post = fork_post_set_flag,
            .env = &env,
        },
        .fork = {
            .enable = true,
        },
    };

    res = theft_run(&cfg);
    ASSERT_EQ_FMTm("hook_flag is set on child process, unmodified due to COW",
        false, env.hook_flag, "%d");
    ASSERT_ENUM_EQ(THEFT_RUN_PASS, res, theft_run_res_str);
    PASS();
}

static size_t Fibonacci(uint16_t x) {
    if (x < 2) {
        return 1;
    } else {
        return Fibonacci(x - 1) + Fibonacci(x - 2);
    }
}

static enum theft_trial_res
prop_too_much_cpu(struct theft *t, void *arg1) {
    (void)t;
    uint16_t value = *(uint16_t *)arg1;

    value = value % 1000;
    fprintf(stderr, "Checking for excess CPU usage with depth %u...\n", value);

    size_t res = Fibonacci(value);  /* Burn CPU by recursing  */

    /* This check is mainly so the call to Fibonacci won't get compiled
     * away. As far as I know, none of the first 1000 Fibonacci series
     * numbers equal 0 when truncated to a size_t, but I'll be pretty
     * impressed if any compilers figure that out. */
    if (res == 0) { return THEFT_TRIAL_ERROR; }

    return THEFT_TRIAL_PASS;
}

static enum theft_hook_fork_post_res
fork_post_rlimit_cpu(const struct theft_hook_fork_post_info *info, void *env) {
    (void)info;
    (void)env;

    struct rlimit rl;
    if (-1 == getrlimit(RLIMIT_CPU, &rl)) {
        perror("getrlimit");
        return THEFT_HOOK_FORK_POST_ERROR;
    }

    /* Restrict property test to 1 second of CPU time. */
    rl.rlim_cur = rl.rlim_max = 1;

    if (-1 == setrlimit(RLIMIT_CPU, &rl)) {
        perror("setrlimit");
        return THEFT_HOOK_FORK_POST_ERROR;
    }

    return THEFT_HOOK_FORK_POST_CONTINUE;
}

/* Fork a child process, set a resource limit on CPU usage time,
 * and use shrinking to determine the smallest double-recursive
 * Fibonacci number calculation that takes over a second of CPU. */
TEST forking_privilege_drop_cpu_limit__slow(void) {
    struct theft_run_config cfg = {
        .name = __func__,
        .prop1 = prop_too_much_cpu,
        .type_info = { theft_get_builtin_type_info(THEFT_BUILTIN_uint16_t) },
        .trials = 1000,
        .seed = theft_seed_of_time(),
        .hooks = {
            .trial_pre = theft_hook_first_fail_halt,
            .fork_post = fork_post_rlimit_cpu,
        },
        .fork = {
            .enable = true,
        },
    };

    enum theft_run_res res = theft_run(&cfg);
    ASSERT_ENUM_EQm("should fail due to CPU limit",
        THEFT_RUN_FAIL, res, theft_run_res_str);
    PASS();
}

struct arg_check_env {
    uint8_t tag;
    uint16_t value;
    bool match;
};

static enum theft_trial_res
prop_even(struct theft *t, void *arg1) {
    struct arg_check_env *env = theft_hook_get_env(t);
    assert(env->tag == 'A');

    uint16_t v = *(uint16_t *)arg1;
    env->value = v;

    return (v & 1 ? THEFT_TRIAL_FAIL : THEFT_TRIAL_PASS);
}

static enum theft_hook_trial_post_res
check_arg_is_odd(const struct theft_hook_trial_post_info *info,
        void *void_env) {
    struct arg_check_env *env = void_env;

    if (info->result == THEFT_TRIAL_FAIL) {
        uint16_t v = *(uint16_t *)info->args[0];
        if (v == env->value) {
            env->match = true;
        }
    }

    return THEFT_HOOK_TRIAL_POST_CONTINUE;
}

TEST trial_post_hook_gets_correct_args(void) {
    struct arg_check_env env = { .tag = 'A', };

    struct theft_run_config cfg = {
        .name = __func__,
        .prop1 = prop_even,
        .type_info = { theft_get_builtin_type_info(THEFT_BUILTIN_uint16_t) },
        .trials = 100,
        .seed = theft_seed_of_time(),
        .hooks = {
            .trial_post = check_arg_is_odd,
            .env = &env,
        },
    };

    enum theft_run_res res = theft_run(&cfg);
    ASSERT_ENUM_EQm("should fail", THEFT_RUN_FAIL, res, theft_run_res_str);
    ASSERTm("value seen by trial_post hook did not match",
        env.match);
    PASS();
}

static struct theft_type_info uint_type_info_no_free = {
    .alloc = uint_alloc,
    .free = NULL,               /* intentionally missing */
    .print = uint_print,
    .autoshrink_config = {
        .enable = true,
    },
};

static enum theft_trial_res
prop_triskaidekaphobia(struct theft *t, void *arg1) {
    (void)t;
    uint32_t v = *(uint32_t *)arg1;
    return ((v % 13) == 0 ? THEFT_TRIAL_FAIL : THEFT_TRIAL_PASS);
}

TEST free_callback_should_be_optional(void) {
    struct theft_run_config cfg = {
        .name = __func__,
        .prop1 = prop_triskaidekaphobia,
        /* Not using a built-in so .free can be NULL. */
        .type_info = { &uint_type_info_no_free },
        .trials = 1000,
        .seed = theft_seed_of_time(),
    };

    enum theft_run_res res = theft_run(&cfg);
    ASSERTm("FAIL is likely, PASS is okay, but don't crash",
        res == THEFT_RUN_FAIL || res == THEFT_RUN_PASS);
    PASS();
}

SUITE(integration) {
    RUN_TEST(generated_unsigned_ints_are_positive);
    RUN_TEST(generated_int_list_with_cons_is_longer);
    RUN_TEST(generated_int_list_does_not_repeat_values);
    RUN_TEST(two_generated_lists_do_not_match);
    RUN_TEST(always_seeds_must_be_run);
    RUN_TEST(overconstrained_state_spaces_should_be_detected);

    /* Tests for hook_cb functionality */
    RUN_TEST(save_seed_and_error_before_generating_args);
    RUN_TEST(gen_pre_halt);
    RUN_TEST(only_shrink_three_times);
    RUN_TEST(save_local_minimum_and_re_run);
    RUN_TEST(repeat_local_minimum_once);
    RUN_TEST(repeat_first_successful_shrink_once_then_halt);

    /* Tests for forking/timeouts */
    RUN_TEST(shrink_crash);
    RUN_TEST(shrink_infinite_loop);
    RUN_TEST(shrink_abort_immediately_to_stress_forking__slow);
    RUN_TEST(shrink_and_SIGUSR1_on_timeout);
    RUN_TEST(shrink_and_SIGUSR1_on_timeout_then_SIGKILL);
    RUN_TEST(forking_hook);
    RUN_TEST(forking_privilege_drop_cpu_limit__slow);

    RUN_TEST(repeat_with_verbose_set_after_shrinking);

    // Regressions
    RUN_TEST(expected_seed_should_be_used_first);
    RUN_TEST(trial_post_hook_gets_correct_args);
    RUN_TEST(free_callback_should_be_optional);
}
