#include "test_theft.h"
#include "test_theft_autoshrink_ll.h"

#include <sys/time.h>

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

    //printf("PASS\n");
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
    size_t failures;
};

static enum theft_hook_res
hook(const struct theft_hook_info *info, void *penv) {
    struct hook_env *env = (struct hook_env *)penv;

    if (info->type == THEFT_HOOK_TYPE_TRIAL_POST &&
        info->u.trial_post.result == THEFT_TRIAL_FAIL) {
        env->failures++;
    } else if (info->type == THEFT_HOOK_TYPE_TRIAL_PRE) {
        if (env->failures == 10) {
            return THEFT_HOOK_HALT;
        }
    }
    return THEFT_HOOK_CONTINUE;
}

TEST ll_prop(size_t seed, const char *name, theft_propfun *prop) {
    struct theft *t = theft_init(NULL);
    enum theft_run_res res;

    struct hook_env env = { .failures = 0 };

    struct theft_run_config cfg = {
        .name = name,
        .fun = prop,
        .type_info = { &ll_info },
        .hook_cb = hook,
        .env = &env,
        .trials = 50000,
        .seed = seed,
    };

    res = theft_run(t, &cfg);
    ASSERT_EQm("should find counter-examples", THEFT_RUN_FAIL, res);
    theft_free(t);
    PASS();
}

SUITE(autoshrink) {
    struct timeval tv;

    if (-1 == gettimeofday(&tv, NULL)) {
        assert(false);
    }

    theft_seed seed = tv.tv_sec ^ tv.tv_usec;

    RUN_TESTp(ll_prop, seed, "no duplicates", prop_no_duplicates);
    RUN_TESTp(ll_prop, seed, "not ascending", prop_not_ascending);
    RUN_TESTp(ll_prop, seed, "no dupes with a non-zero value between",
        prop_no_dupes_with_value_between);
    RUN_TESTp(ll_prop, seed, "no non-zero numbers followed by their square",
        prop_no_nonzero_numbers_followed_by_their_square);
    RUN_TESTp(ll_prop, seed, "no sequence of three numbers",
        prop_no_seq_of_3);
}
