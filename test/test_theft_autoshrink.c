#include "test_theft.h"

#include <assert.h>

struct ll {
    char tag;
    uint8_t value;
    struct ll *next;
};

static void ll_print(FILE *f, const void *instance, void *env);

static enum theft_trial_res
prop_no_dupes_with_value_between(void *arg) {
    struct ll *head = (struct ll *)arg;
    assert(head->tag == 'L');

    struct ll *cur = head->next;
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
    
    //printf("PASS\n");
    return THEFT_TRIAL_PASS;
}

static enum theft_alloc_res
ll_alloc(struct theft *t, void *env, void **instance) {
    (void)env;
    struct ll *res = calloc(1, sizeof(struct ll));
    if (res == NULL) {
        return THEFT_ALLOC_ERROR;
    }

    res->tag = 'L';
    res->value = (uint8_t)theft_random_bits(t, 8);

    /* Each link, have a 1 in 8 chance of end of list */
    while (theft_random_bits(t, 3) != 0x00) {
        struct ll *link = calloc(1, sizeof(struct ll));
        if (res == NULL) {
            return THEFT_ALLOC_ERROR;
        }
        link->tag = 'L';
        link->value = (uint8_t)theft_random_bits(t, 8);
        link->next = res;
        res = link;
    }
    
    *instance = res;
    /* fprintf(stdout, "ALLOC: "); */
    /* ll_print(stdout, res, NULL); */
    /* fprintf(stdout, "\n"); */
    return THEFT_ALLOC_OK;
}

static void
ll_free(void *instance, void *env) {
    (void)env;
    struct ll *cur = (struct ll *)instance;
    assert(cur->tag == 'L');

    while (cur) {
        struct ll *next = cur->next;
        free(cur);
        cur = next;
    }
}

static void ll_print(FILE *f, const void *instance, void *env) {
    (void)env;
    const struct ll *cur = (struct ll *)instance;

    fprintf(f, "[");
    while (cur) {
        assert(cur->tag == 'L');
        const struct ll *next = cur->next;
        fprintf(f, "%u ", cur->value);
        cur = next;
    }
    fprintf(f, "]");
}

static struct theft_type_info ll_info = {
    .alloc = ll_alloc,
    .free = ll_free,
    .print = ll_print,
    .autoshrink = true,
};

TEST autoshrink_ll_just_alloc_and_free(void) {
    struct theft *t = theft_init(NULL);
    enum theft_run_res res;

    (void)ll_free;
    (void)ll_print;

    struct theft_run_config cfg = {
        .name = __func__,
        .fun = prop_no_dupes_with_value_between,
        .type_info = { &ll_info },
        //.hook_cb = ll_just_alloc_and_free_hook,
        .trials = 1000,
        .seed = 12345,
    };

    res = theft_run(t, &cfg);
    ASSERT_EQm("should find counter-examples", THEFT_RUN_FAIL, res);
    theft_free(t);
    PASS();
}

SUITE(autoshrink) {
    RUN_TEST(autoshrink_ll_just_alloc_and_free);
}
