#include "greatest.h"
#include "theft.h"

#include <sys/time.h>

#define COUNT(X) (sizeof(X)/sizeof(X[0]))

/* Add typedefs to abbreviate these. */
typedef struct theft theft;
typedef struct theft_cfg theft_cfg;
typedef struct theft_type_info theft_type_info;
typedef struct theft_propfun_info theft_propfun_info;

TEST alloc_and_free() {
    theft *t = theft_init(0);

    theft_free(t);
    PASS();
}

static void *uint_alloc(theft *t, theft_seed s, void *env) {
    uint32_t *n = malloc(sizeof(uint32_t));
    if (n == NULL) { return THEFT_ERROR; }
    *n = (uint32_t)(s & 0xFFFFFFFF);
    (void)t; (void)env;
    return n;
}

static void uint_free(void *p, void *env) {
    (void)env;
    free(p);
}

static void uint_print(FILE *f, void *p, void *env) {
    (void)env;
    fprintf(f, "%u", *(uint32_t *)p);
}

static theft_type_info uint = {
    .alloc = uint_alloc,
    .free = uint_free,
    .print = uint_print,
};

static theft_progress_callback_res
guiap_prog_cb(struct theft_trial_info *info, void *env) {
    (void)info; (void)env;
    return THEFT_PROGRESS_CONTINUE;
}

static theft_trial_res is_pos(uint32_t *n) {
    if ((*n) >= 0) {
        return THEFT_TRIAL_PASS;
    } else {
        return THEFT_TRIAL_FAIL;
    }
}

TEST generated_unsigned_ints_are_positive() {
    theft *t = theft_init(0);

    theft_run_res res;

    /* The configuration struct can be passed in as an argument literal,
     * though you have to cast it. */
    res = theft_run(t, &(struct theft_cfg){
            .name = "generated_unsigned_ints_are_positive",
            .fun = is_pos,
            .type_info = { &uint },
            .progress_cb = guiap_prog_cb,
        });
    ASSERT_EQm("generated_unsigned_ints_are_positive", THEFT_RUN_PASS, res);
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

static void list_unpack_seed(theft_hash seed, int32_t *lower, uint32_t *upper) {
    *lower = (int32_t)(seed & 0xFFFFFFFF);
    *upper = (uint32_t)((seed >> 32) & 0xFFFFFFFF);

}

static void *list_alloc(theft *t, theft_seed seed, void *env) {
    (void)env;
    list *l = NULL;             /* empty */

    int32_t lower = 0;
    uint32_t upper = 0;
    int len = 0;

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

    return l;
}

static theft_hash list_hash(void *instance, void *env) {
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

static list *split_list_copy(list *l, bool first_half) {
    int len = list_length(l);
    if (len < 2) { return THEFT_DEAD_END; } 
    list *nl = copy_list(l);
    if (nl == NULL) { return THEFT_ERROR; }
    list *t = nl;
    for (int i = 0; i < len/2 - 1; i++) { t = t->next; }

    list *tail = t->next;
    t->next = NULL;
    if (first_half) {
        list_free(tail, NULL);
        return nl;
    } else {
        list_free(nl, NULL);
        return tail;
    }
}

static void *list_shrink(void *instance, uint32_t tactic, void *env) {
    list *l = (list *)instance;
    if (l == NULL) { return THEFT_NO_MORE_TACTICS; }

    /* When reducing, it's faster to have the tactics ordered by how
     * much they simplify the instance, if possible. In this case, we
     * first try discarding either half of the list, then dividing the
     * whole list by 2, before operations that only impact one element
     * of the list. */

    if (tactic == 0) {          /* first half */
        return split_list_copy(l, true);
    } else if (tactic == 1) {   /* second half */
        return split_list_copy(l, false);
    } else if (tactic == 2) {      /* div whole list by 2 */
        bool nonzero = false;
        for (list *link = l; link; link = link->next) {
            if (link->v > 0) { nonzero = true; break; }
        }

        if (nonzero) {
            list *nl = copy_list(l);
            if (nl == NULL) { return THEFT_ERROR; }

            for (list *link = nl; link; link = link->next) {
                link->v /= 2;
            }
            return nl;
        } else {
            return THEFT_DEAD_END;
        }
    } else if (tactic == 3) {      /* drop head */
        if (l->next == NULL) { return THEFT_DEAD_END; }
        list *nl = copy_list(l->next);
        if (nl == NULL) { return THEFT_ERROR; }
        list *nnl = nl->next;
        nl->next = NULL;
        list_free(nl, NULL);
        return nnl;
    } else if (tactic == 4) {      /* drop tail */
        if (l->next == NULL) { return THEFT_DEAD_END; }

        list *nl = copy_list(l);
        if (nl == NULL) { return THEFT_ERROR; }
        list *prev = nl;
        list *tl = nl;
        
        while (tl->next) {
            prev = tl;
            tl = tl->next;
        }
        prev->next = NULL;
        list_free(tl, NULL);
        return nl;        
    } else {
        (void)instance;
        (void)tactic;
        (void)env;
        return THEFT_NO_MORE_TACTICS;
    }
}

static void list_print(FILE *f, void *instance, void *env) {
    fprintf(f, "(");
    list *l = (list *)instance;
    while (l) {
        fprintf(f, "%s%d", l == instance ? "" : " ", l->v);
        l = l->next;
    }
    fprintf(f, ")");
    (void)env;
}

static theft_type_info list_info = {
    .alloc = list_alloc,
    .free = list_free,
    .hash = list_hash,
    .shrink = list_shrink,
    .print = list_print,
};

static theft_trial_res prop_gen_cons(list *l) {
    list *nl = malloc(sizeof(list));
    if (nl == NULL) { return THEFT_TRIAL_ERROR; }
    nl->v = 0;
    nl->next = l;

    theft_trial_res res;
    if (list_length(nl) == list_length(l) + 1) {
        res = THEFT_TRIAL_PASS;
    } else {
        res = THEFT_TRIAL_FAIL;
    }
    free(nl);
    return res;
}

static theft_progress_callback_res
gilwcil_prog_cb(struct theft_trial_info *info, void *env) {
    (void)info; (void)env;
    return THEFT_PROGRESS_CONTINUE;
}

TEST generated_int_list_with_cons_is_longer() {
    theft *t = theft_init(0);
    theft_run_res res;
    struct theft_cfg cfg = {
        .name = __func__,
        .fun = prop_gen_cons,
        .type_info = { &list_info },
        .progress_cb = gilwcil_prog_cb,
    };
    res = theft_run(t, &cfg);
              
    ASSERT_EQ(THEFT_RUN_PASS, res);
    theft_free(t);
    PASS();
}

static theft_trial_res prop_gen_list_unique(list *l) {
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

static theft_progress_callback_res
gildnrv_prog_cb(struct theft_trial_info *info, void *env) {
    (void)info; (void)env;
    if (info->trial % 100 == 0) { printf("."); fflush(stdout); }
    return THEFT_PROGRESS_CONTINUE;
}

TEST generated_int_list_does_not_repeat_values() {
    /* This test is expected to fail, with meaningful counter-examples. */

    struct theft_trial_report report;

    theft *t = theft_init(0);
    theft_run_res res;
    struct theft_cfg cfg = {
        .name = __func__,
        .fun = prop_gen_list_unique,
        .type_info = { &list_info },
        .progress_cb = gildnrv_prog_cb,
        .report = &report,
        .trials = 1000,
        .seed = 12345,
    };
    
    res = theft_run(t, &cfg);
    printf(" -- PASS %zd, FAIL %zd, SKIP %zd\n",
        report.pass, report.fail, report.skip);
    ASSERT_EQm("should find counter-examples", THEFT_RUN_FAIL, res);
    ASSERT(report.fail > 0);
    theft_free(t);
    PASS();
}

TEST prng_should_return_same_series_from_same_seeds() {
    theft_seed seeds[8];
    theft_seed values[8][8];

    theft *t = theft_init(0);

    /* Set for deterministic start */
    theft_set_seed(t, 0xabad5eed);
    for (int i = 0; i < 8; i++) {
        seeds[i] = theft_random(t);
    }

    /* Populate value tables. */
    for (int s = 0; s < 8; s++) {
        theft_set_seed(t, seeds[s]);
        for (int i = 0; i < 8; i++) {
            values[s][i] = theft_random(t);
        }
    }

    /* Check values. */
    for (int s = 0; s < 8; s++) {
        theft_set_seed(t, seeds[s]);
        for (int i = 0; i < 8; i++) {
            ASSERT_EQ(values[s][i], theft_random(t));
        }
    }
    theft_free(t);
    PASS();
}

static theft_trial_res
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

typedef struct {
    int dots;
} test_env;

static theft_progress_callback_res
prog_cb(struct theft_trial_info *info, void *env) {
    test_env *e = (test_env *)env;

    if (info->trial % 100 == 0) {
        printf(".");
        e->dots++;
        if (e->dots == 72) {
            e->dots = 0;
            printf("\n");
        }
        fflush(stdout);
    }
    return THEFT_PROGRESS_CONTINUE;
}

TEST two_generated_lists_do_not_match() {
    /* This test is expected to fail, with meaningful counter-examples. */
    test_env env;
    memset(&env, 0, sizeof(env));
    struct theft_trial_report report;

    struct timeval tv;
    if (-1 == gettimeofday(&tv, NULL)) { FAIL(); }

    theft *t = theft_init(0);
    ASSERT(t);
    theft_run_res res;
    struct theft_cfg cfg = {
        .name = __func__,
        .fun = prop_gen_list_unique_pair,
        .type_info = { &list_info, &list_info },
        .trials = 10000,
        .progress_cb = prog_cb,
        .env = &env,
        .report = &report,
        .seed = (theft_seed)(tv.tv_sec ^ tv.tv_usec)
    };
    res = theft_run(t, &cfg);
    ASSERT_EQm("should find counter-examples", THEFT_RUN_FAIL, res);
    ASSERT(report.fail > 0);
    theft_free(t);
    PASS();
}

typedef struct {
    int dots;
    int checked;
} always_seed_env;

static theft_progress_callback_res
always_prog_cb(struct theft_trial_info *info, void *venv) {
    always_seed_env *env = (always_seed_env *)venv;

    if (info->trial % 100 == 0) {
        printf(".");
        env->dots++;
        if (env->dots == 72) {
            env->dots = 0;
            printf("\n");
        }
        fflush(stdout);
    }

    /* Must run 'always' seeds */
    if (info->seed == 0x600d5eed) { env->checked |= 0x01; }
    if (info->seed == 0xabad5eed) { env->checked |= 0x02; }

    /* Must also otherwise start from specified seed */
    if (info->seed == 0x600dd06) { env->checked |= 0x04; }

    return THEFT_PROGRESS_CONTINUE;
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

    struct theft_trial_report report;

    theft *t = theft_init(0);
    theft_run_res res;
    struct theft_cfg cfg = {
        .name = __func__,
        .fun = prop_gen_list_unique,
        .type_info = { &list_info },
        .trials = 1000,
        .seed = 0x600dd06,
        ALWAYS_SEEDS(always_seeds),
        .env = &env,
        .progress_cb = always_prog_cb,
        .report = &report,
    };
    res = theft_run(t, &cfg);
    printf(" -- PASS %zd, FAIL %zd, SKIP %zd, DUP %zd\n",
        report.pass, report.fail, report.skip, report.dup);
    ASSERT_EQm("should find counter-examples", THEFT_RUN_FAIL, res);
    ASSERT(report.fail > 0);
    theft_free(t);

    if (0x03 != (env.checked & 0x03)) { FAILm("'always' seeds were not run"); }
    if (0x04 != (env.checked & 0x04)) { FAILm("starting seed was not run"); }
    PASS();
}

#define EXPECTED_SEED 0x15a600d16b175eedL

static void *seed_cmp_alloc(theft *t, theft_seed seed, void *env) {
    bool *res = malloc(sizeof(*res));
    (void)env;
    (void)t;

    theft_seed r1 = theft_random(t);

    /* A seed and the same seed with the upper 32 bits all set should
     * not lead to the same random value. */
    theft_seed seed_eq_lower_4_bytes = seed | 0xFFFFFFFF00000000L;
    theft_set_seed(t, seed_eq_lower_4_bytes);
    
    theft_seed r2 = theft_random(t);

    if (res) {
        *res = (r1 != r2);
        return res;
    } else {
        return THEFT_ERROR;
    }
}

static void seed_cmp_free(void *instance, void *env) {
    (void)env;
    free(instance);
}

static theft_type_info seed_cmp_info = {
    .alloc = seed_cmp_alloc,
    .free = seed_cmp_free,
};

static theft_trial_res
prop_saved_seeds(bool *res) {
    if (*res == true) {
        return THEFT_TRIAL_PASS;
    } else {
        return THEFT_TRIAL_FAIL;
    }
}

TEST always_seeds_should_not_be_truncated(void) {
    /* This isn't really a property test so much as a test checking
     * that explicitly requested 64-bit seeds are passed through to the
     * callbacks without being truncated. */
    theft *t = theft_init(0);

    theft_cfg cfg = {
        .fun = prop_saved_seeds,
        .type_info = { &seed_cmp_info },
        .trials = 1,
        .seed = EXPECTED_SEED,
    };

    theft_run_res res = theft_run(t, &cfg);
    ASSERT_EQ(THEFT_RUN_PASS, res);
    theft_free(t);
    PASS();
}

static void *seed_alloc(theft *t, theft_seed seed, void *env) {
    theft_seed *res = malloc(sizeof(*res));
    if (res == NULL) { return THEFT_ERROR; }
    (void)env;
    (void)t;
    *res = seed;
    return res;
}

static void seed_free(void *instance, void *env) {
    (void)env;
    free(instance);
}

static theft_type_info seed_info = {
    .alloc = seed_alloc,
    .free = seed_free,
};

static theft_trial_res
prop_expected_seed_is_generated(theft_seed *s) {
    if (*s == EXPECTED_SEED) {
        return THEFT_TRIAL_PASS;
    } else {
        return THEFT_TRIAL_FAIL;
    }
}

TEST always_seeds_should_be_used_first(void) {
    theft *t = theft_init(0);

    theft_cfg cfg = {
        .fun = prop_expected_seed_is_generated,
        .type_info = { &seed_info },
        .trials = 1,
        .seed = EXPECTED_SEED,
    };

    theft_run_res res = theft_run(t, &cfg);
    ASSERT_EQ(THEFT_RUN_PASS, res);
    theft_free(t);
    PASS();
}

static theft_trial_res
prop_bool_tautology(bool *bp) {
    bool b = *bp;
    if (b || !b) {    // tautology to force shrinking
        return THEFT_TRIAL_FAIL;
    } else {
        return THEFT_TRIAL_PASS;
    }
}

static void *bool_alloc(theft *t, theft_seed seed, void *env) {
    bool *bp = malloc(sizeof(*bp));
    if (bp == NULL) { return THEFT_ERROR; }
    *bp = (seed & 0x01 ? true : false);
    (void)env;
    (void)t;
    return bp;
}

static void bool_free(void *instance, void *env) {
    (void)env;
    free(instance);
}

static theft_hash bool_hash(void *instance, void *env) {
    bool *bp = (bool *)instance;
    bool b = *bp;
    (void)env;
    return (theft_hash)(b ? 1 : 0);
}

static theft_type_info bool_info = {
    .alloc = bool_alloc,
    .free = bool_free,
    .hash = bool_hash,
};

TEST overconstrained_state_spaces_should_be_detected(void) {
    theft *t = theft_init(0);
    struct theft_trial_report report;

    theft_cfg cfg = {
        .fun = prop_bool_tautology,
        .type_info = { &bool_info },
        .report = &report,
        .trials = 100,
    };

    theft_run_res res = theft_run(t, &cfg);
    theft_free(t);
    ASSERT_EQ(THEFT_RUN_FAIL, res);
    ASSERT_EQ(2, report.fail);
    ASSERT_EQ(98, report.dup);
    PASS();
}

SUITE(suite) {
    RUN_TEST(alloc_and_free);
    RUN_TEST(generated_unsigned_ints_are_positive);
    RUN_TEST(generated_int_list_with_cons_is_longer);
    RUN_TEST(generated_int_list_does_not_repeat_values);
    RUN_TEST(prng_should_return_same_series_from_same_seeds);
    RUN_TEST(two_generated_lists_do_not_match);
    RUN_TEST(always_seeds_must_be_run);
    RUN_TEST(overconstrained_state_spaces_should_be_detected);

    // Regressions
    RUN_TEST(always_seeds_should_not_be_truncated);
    RUN_TEST(always_seeds_should_be_used_first);
}

/* Add all the definitions that need to be in the test runner's main file. */
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();      /* command-line arguments, initialization. */
    RUN_SUITE(suite);
    GREATEST_MAIN_END();        /* display results */
}
