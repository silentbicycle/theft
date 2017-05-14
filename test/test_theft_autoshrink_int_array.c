#include "test_theft.h"
#include "test_theft_autoshrink_int_array.h"

static void ia_print(FILE *f, const void *instance, void *env);

#define IA_MAX 256

static enum theft_alloc_res
ia_alloc(struct theft *t, void *env, void **instance) {
    (void)env;

    uint8_t *ints = calloc(IA_MAX, sizeof(uint8_t));
    if (ints == NULL) {
        return THEFT_ALLOC_ERROR;
    }

    for (size_t i = 0; i < IA_MAX; i++) {
        uint8_t v = theft_random_bits(t, 8);
        if (v == 0) {
            break;
        }
        ints[i] = v;
    }

    *instance = ints;
    //ia_print(stdout, ints, NULL); printf("\n");
    return THEFT_ALLOC_OK;
}

static void ia_free(void *instance, void *env) {
    (void)env;
    free(instance);
}

static void ia_print(FILE *f, const void *instance, void *env) {
    uint8_t *ia = (uint8_t *)instance;
    (void)env;
    fprintf(f, "[");
    for (size_t i = 0; i < IA_MAX; i++) {
        if (ia[i] == 0) {
            break;
        }
        fprintf(f, "%u, ", ia[i]);
        if ((i > 0) && (i % 16) == 0) {
            fprintf(f, "\n");
        }
    }
    fprintf(f, "]");
}

struct theft_type_info ia_info = {
    .alloc = ia_alloc,
    .free = ia_free,
    .print = ia_print,
    .autoshrink_config = {
        .enable = true,
    },
};

