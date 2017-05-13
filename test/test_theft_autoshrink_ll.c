#include "test_theft.h"
#include "test_theft_autoshrink_ll.h"

static void ll_print(FILE *f, const void *instance, void *env);

static enum theft_alloc_res
ll_alloc(struct theft *t, void *env, void **instance) {
    (void)env;
    struct ll *res = NULL;

    /* Each link, have a 1 in 8 chance of end of list */
    while (theft_random_bits(t, 3) != 0x00) {
        struct ll *link = calloc(1, sizeof(struct ll));
        if (link == NULL) {
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

    while (cur) {
        assert(cur->tag == 'L');
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

struct theft_type_info ll_info = {
    .alloc = ll_alloc,
    .free = ll_free,
    .print = ll_print,
    .autoshrink = true,
};
