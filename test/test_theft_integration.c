#include "test_theft.h"

#include <sys/time.h>
#include <assert.h>
#include <inttypes.h>

#define COUNT(X) (sizeof(X)/sizeof(X[0]))

TEST alloc_and_free() {
    struct theft *t = theft_init(NULL);

    theft_free(t);
    PASS();
}

static enum theft_alloc_res
uint_alloc(struct theft *t, void *env, void **output) {
    uint32_t *n = malloc(sizeof(uint32_t));
    if (n == NULL) { return THEFT_ALLOC_ERROR; }
    *n = (uint32_t)(theft_random(t) & 0xFFFFFFFF);
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

static enum theft_trial_res is_pos(uint32_t *n) {
    if ((*n) >= 0) {  // tautological
        return THEFT_TRIAL_PASS;
    } else {
        return THEFT_TRIAL_FAIL;
    }
}

TEST generated_unsigned_ints_are_positive() {
    struct theft *t = theft_init(NULL);

    enum theft_run_res res;

    /* The configuration struct can be passed in as an argument literal,
     * though you have to cast it. */
    res = theft_run(t, &(struct theft_run_config){
            .name = "generated_unsigned_ints_are_positive",
            .fun = is_pos,
            .type_info = { &uint_type_info },
        });
    ASSERT_EQm("generated_unsigned_ints_are_positive",
        THEFT_RUN_PASS, res);
    theft_free(t);
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

    theft_seed seed = theft_random(t);
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

        seed = theft_random(t);
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

static enum theft_trial_res prop_gen_cons(list *l) {
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

TEST generated_int_list_with_cons_is_longer() {
    struct theft *t = theft_init(NULL);
    enum theft_run_res res;
    struct theft_run_config cfg = {
        .name = __func__,
        .fun = prop_gen_cons,
        .type_info = { &list_info },
    };
    res = theft_run(t, &cfg);

    ASSERT_EQ(THEFT_RUN_PASS, res);
    theft_free(t);
    PASS();
}

static enum theft_trial_res
prop_gen_list_unique(list *l) {
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

TEST generated_int_list_does_not_repeat_values() {
    /* This test is expected to fail, with meaningful counter-examples. */
    struct theft *t = theft_init(NULL);

    struct test_env env = { .fail = false };

    struct theft_run_config cfg = {
        .name = __func__,
        .fun = prop_gen_list_unique,
        .type_info = { &list_info },
        .hooks = {
            .trial_post = gildnrv_trial_post_hook,
            .env = &env,
        },
        .trials = 1000,
        .seed = 12345,
    };

    enum theft_run_res res;
    res = theft_run(t, &cfg);
    ASSERT_EQ_FMTm("should find counter-examples", THEFT_RUN_FAIL, res, "%d");
    ASSERT(env.fail > 0);
    theft_free(t);
    PASS();
}

static enum theft_trial_res
prop_gen_list_unique_pair(list *a, list *b) {
    if (list_length(a) == list_length(b)) {
        list *la = a;
        list *lb = b;

        for (la = a; la && lb; la = a->next, lb = b->next) {
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

TEST two_generated_lists_do_not_match() {
    /* This test is expected to fail, with meaningful counter-examples. */
    struct test_env env;
    memset(&env, 0, sizeof(env));

    struct timeval tv;
    if (-1 == gettimeofday(&tv, NULL)) { FAIL(); }

    struct theft *t = theft_init(NULL);
    ASSERT(t);
    enum theft_run_res res;
    struct theft_run_config cfg = {
        .name = __func__,
        .fun = prop_gen_list_unique_pair,
        .type_info = { &list_info, &list_info },
        .trials = 10000,
        .hooks = {
            .trial_post = trial_post_hook_cb,
            .env = &env,
        },
        .seed = (theft_seed)(tv.tv_sec ^ tv.tv_usec)
    };
    res = theft_run(t, &cfg);
    ASSERT_EQm("should find counter-examples", THEFT_RUN_FAIL, res);
    ASSERT(env.fail > 0);
    theft_free(t);
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

static enum theft_hook_run_post_res
always_seeds_run_post(const struct theft_hook_run_post_info *info, void *env) {
    (void)env;
    const struct theft_run_report *report = &info->report;
    printf("\n -- PASS %zd, FAIL %zd, SKIP %zd, DUP %zd\n",
        report->pass, report->fail, report->skip, report->dup);
    return THEFT_HOOK_RUN_POST_CONTINUE;
}

/* Or, the optional always_seed fields could be wrapped in a macro... */
#define ALWAYS_SEEDS(A)                                               \
      .always_seed_count = COUNT(A),                                  \
      .always_seeds = A

TEST always_seeds_must_be_run() {
    /* This test is expected to fail, with meaningful counter-examples. */
    static theft_hash always_seeds[] = {
        0x600d5eed, 0xabad5eed,
    };

    always_seed_env env;
    memset(&env, 0, sizeof(env));

    struct theft *t = theft_init(NULL);
    enum theft_run_res res;
    struct theft_run_config cfg = {
        .name = __func__,
        .fun = prop_gen_list_unique,
        .type_info = { &list_info },
        .trials = 1000,
        .seed = 0x600dd06,
        ALWAYS_SEEDS(always_seeds),
        .hooks = {
            .trial_post = always_seeds_trial_post,
            .run_post = always_seeds_run_post,
            .env = &env,
        },
    };
    res = theft_run(t, &cfg);
    ASSERT_EQm("should find counter-examples", THEFT_RUN_FAIL, res);
    ASSERT(env.fail > 0);
    theft_free(t);

    if (0x03 != (env.checked & 0x03)) { FAILm("'always' seeds were not run"); }
    if (0x04 != (env.checked & 0x04)) { FAILm("starting seed was not run"); }
    PASS();
}

#define EXPECTED_SEED 0x15a600d64b175eedLL

TEST seeds_should_not_be_truncated(void) {
    /* This isn't really a property test so much as a test checking
     * that explicitly requested 64-bit seeds are passed through to the
     * callbacks without being truncated. */
    SKIPm("FIXME");
}

static enum theft_alloc_res
seed_alloc(struct theft *t, void *env, void **output) {
    theft_seed *res = malloc(sizeof(*res));
    if (res == NULL) { return THEFT_ALLOC_ERROR; }
    (void)env;
    (void)t;
    *res = theft_random(t);
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

static enum theft_trial_res
prop_expected_seed_is_generated(theft_seed *s) {
    if (*s == EXPECTED_SEED) {
        return THEFT_TRIAL_PASS;
    } else {
        return THEFT_TRIAL_FAIL;
    }
}

TEST expected_seed_should_be_used_first(void) {
    struct theft *t = theft_init(NULL);

    struct theft_run_config cfg = {
        .fun = prop_expected_seed_is_generated,
        .type_info = { &seed_info },
        .trials = 1,
        .seed = EXPECTED_SEED,
    };

    enum theft_run_res res = theft_run(t, &cfg);
    ASSERT_EQ(THEFT_RUN_PASS, res);
    theft_free(t);
    PASS();
}

static enum theft_trial_res
prop_bool_tautology(bool *bp) {
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
    theft_seed seed = theft_random(t);
    *bp = (seed & 0x01 ? true : false);
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
    struct theft *t = theft_init(NULL);

    struct theft_run_report report = {
        .pass = 0,
    };

    struct theft_run_config cfg = {
        .fun = prop_bool_tautology,
        .type_info = { &bool_info },
        .trials = 100,
        .hooks = {
            .run_post = save_report_run_post,
            .env = (void *)&report,
        },
    };

    enum theft_run_res res = theft_run(t, &cfg);
    theft_free(t);
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

static enum theft_trial_res should_never_run(void *x) {
    (void)x;
    assert(false);
    return THEFT_TRIAL_PASS;
}

TEST save_seed_and_error_before_generating_args(void) {
    struct theft *t = theft_init(NULL);

    theft_seed seed = 0;

    struct theft_run_config cfg = {
        .fun = should_never_run,
        .type_info = { &never_run_info },
        .hooks = {
            .gen_args_pre = error_in_gen_args_pre,
            .env = (void *)&seed,
        },
        .seed = 0xf005ba11,
    };

    enum theft_run_res res = theft_run(t, &cfg);
    theft_free(t);

    ASSERT_EQ(THEFT_RUN_ERROR, res);
    ASSERT_EQ_FMT(0xf005ba11, seed, "%lx");

    PASS();
}

static enum theft_trial_res always_pass(void *x) {
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
    struct theft *t = theft_init(NULL);

    struct theft_run_report report = { .pass = 0 };

    struct theft_run_config cfg = {
        .fun = always_pass,
        .type_info = { &uint_type_info },
        .hooks = {
            .trial_pre = halt_before_third_trial_pre,
            .run_post = halt_before_third_run_post,
            .env = (void *)&report,
        },
    };

    enum theft_run_res res = theft_run(t, &cfg);
    theft_free(t);

    ASSERT_EQ(THEFT_RUN_PASS, res);
    ASSERT_EQ_FMT(2, report.pass, "%zd");
    ASSERT_EQ_FMT(0, report.fail, "%zd");
    ASSERT_EQ_FMT(0, report.skip, "%zd");
    ASSERT_EQ_FMT(0, report.dup, "%zd");

    PASS();
}


static enum theft_trial_res
prop_uint_is_lte_12345(void *arg) {
    uint32_t *pnum = (uint32_t *)arg;;

    return *pnum <= 12345 ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
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
    uint32_t value = (theft_random(t) & 0xFFFFFFFF);
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
        *pnum, info->done);
    return THEFT_HOOK_SHRINK_POST_CONTINUE;
}

TEST only_shrink_three_times(void) {
    struct theft *t = theft_init(NULL);

    struct shrink_test_env env = { .shrinks = 0 };

    struct theft_run_config cfg = {
        .fun = prop_uint_is_lte_12345,
        .type_info = { &shrink_test_uint_type_info },
        .trials = 1,
        .hooks = {
            .shrink_pre = halt_after_third_shrink_shrink_pre,
            .shrink_post = halt_after_third_shrink_shrink_post,
            .env = (void *)&env,
        },
    };

    enum theft_run_res res = theft_run(t, &cfg);
    theft_free(t);

    ASSERT_EQ(THEFT_RUN_FAIL, res);
    ASSERT(!env.fail);
    ASSERT_EQ_FMT(3, env.shrinks, "%zd");
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
    if (info->done) {
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
    struct theft *t = theft_init(NULL);

    struct shrink_test_env env = { .shrinks = 0 };

    struct theft_run_config cfg = {
        .fun = prop_uint_is_lte_12345,
        .type_info = { &shrink_test_uint_type_info },
        .hooks = {
            .trial_post = shrink_all_the_way_trial_post,
            .shrink_trial_post = shrink_all_the_way_shrink_trial_post,
            .shrink_post = shrink_all_the_way_shrink_post,
            .env = (void *)&env,
        },
        .trials = 1,
    };

    enum theft_run_res res = theft_run(t, &cfg);
    theft_free(t);

    ASSERT_EQ(THEFT_RUN_FAIL, res);
    ASSERT(!env.fail);
    ASSERT_EQ_FMTm("three trial-post and three shrink-post hook runs",
        33, env.reruns, "%zd");
    ASSERT_EQ_FMT(12346, env.local_minimum, "%zd");
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
    struct theft *t = theft_init(NULL);
    enum theft_run_res res;

    struct repeat_once_env env = { .fail = false };

    struct theft_run_config cfg = {
        .name = __func__,
        .fun = prop_gen_list_unique,
        .type_info = { &list_info },
        .hooks = {
            .trial_pre = repeat_once_trial_pre,
            .trial_post = repeat_once_trial_post,
            .env = &env,
        },
        .trials = 1000,
        .seed = 12345,
    };

    res = theft_run(t, &cfg);
    ASSERT_EQm("should find counter-examples", THEFT_RUN_FAIL, res);
    ASSERT(env.fail);
    ASSERT_EQ_FMT(2, env.local_minimum_runs, "%u");
    theft_free(t);
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
    struct theft *t = theft_init(NULL);
    enum theft_run_res res;

    struct repeat_shrink_once_env env = { .fail = false };

    struct theft_run_config cfg = {
        .name = __func__,
        .fun = prop_gen_list_unique,
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

    res = theft_run(t, &cfg);
    ASSERT_EQm("should find counter-examples", THEFT_RUN_FAIL, res);
    ASSERT(env.fail);
    ASSERT_EQ_FMT(1, env.shrink_repeats, "%u");
    theft_free(t);
    PASS();
}

SUITE(integration) {
    RUN_TEST(alloc_and_free);
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

    // Regressions
    RUN_TEST(seeds_should_not_be_truncated);
    RUN_TEST(expected_seed_should_be_used_first);
}
