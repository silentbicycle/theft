#include "theft_aux.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

struct type_info_row {
    enum theft_builtin_type_info key;
    struct theft_type_info value;
};

void theft_generic_free(void *instance, void *env) {
    (void)env;
    free(instance);
}

static enum theft_alloc_res
bool_alloc(struct theft *t, void *env, void **instance) {
    (void)env;
    bool *res = malloc(sizeof(*res));
    if (res == NULL) { return THEFT_ALLOC_ERROR; }
    *res = (bool)theft_random_bits(t, 1);
    *instance = res;
    return THEFT_ALLOC_OK;
}

#define ALLOC_USCALAR(NAME, TYPE, BITS)                                \
    static enum theft_alloc_res                                        \
    NAME ## _alloc(struct theft *t, void *env, void **instance) {      \
        TYPE *res = malloc(sizeof(*res));                              \
        if (res == NULL) { return THEFT_ALLOC_ERROR; }                 \
        *res = (TYPE)theft_random_bits(t, BITS);                       \
        if (env != NULL) {                                             \
            TYPE *limit = (TYPE *)env;                                 \
            (*res) %= *limit;                                          \
        }                                                              \
        *instance = res;                                               \
        return THEFT_ALLOC_OK;                                         \
    }

#define PRINT_USCALAR(NAME, TYPE, FORMAT)                              \
    static void NAME ## _print(FILE *f,                                \
            const void *instance, void *env) {                         \
        (void)env;                                                     \
        fprintf(f, FORMAT, *(TYPE *)instance);                         \
    }

#define HASH_USCALAR(NAME, TYPE)                                       \
    static theft_hash NAME ## _hash(const void *instance, void *env) { \
        (void)env;                                                     \
        return theft_hash_onepass((uint8_t *)instance, sizeof(TYPE));  \
    }                                                                  \
    
/* Try to shrink a scalar type by subtracting a random amount
 * in each shrinking pass, with several passes. */
#define GENERIC_SHRINK_ATTEMPTS 8
#define SHRINK_USCALAR(NAME, TYPE)                                     \
    static enum theft_shrink_res                                       \
    NAME ## _shrink(struct theft *t, const void *instance,             \
        uint32_t tactic, void *env, void **output) {                   \
        (void)env;                                                     \
        TYPE orig = *(TYPE *)instance;                                 \
        if (orig == 0) {                                               \
            return THEFT_SHRINK_NO_MORE_TACTICS;                       \
        }                                                              \
        for (uint32_t i = 0; i < GENERIC_SHRINK_ATTEMPTS; i++) {       \
            if (tactic == i) {                                         \
                TYPE *res = malloc(sizeof(TYPE));                      \
                if (res == NULL) {                                     \
                    return THEFT_SHRINK_ERROR;                         \
                }                                                      \
                TYPE delta = theft_random_bits(t, 8*sizeof(TYPE));     \
                if (delta == 0) {                                      \
                    return THEFT_SHRINK_DEAD_END;                      \
                } else if (delta == orig) {                            \
                    *res = 0;                                          \
                } else {                                               \
                    *res = orig - (delta % orig);                      \
                }                                                      \
                *output = res;                                         \
                return THEFT_SHRINK_OK;                                \
            }                                                          \
        }                                                              \
        return THEFT_SHRINK_NO_MORE_TACTICS;                           \
    }

ALLOC_USCALAR(uint, unsigned int, 8*sizeof(unsigned int))
ALLOC_USCALAR(uint8_t, uint8_t, 8)
ALLOC_USCALAR(uint16_t, uint16_t, 16)
ALLOC_USCALAR(uint32_t, uint16_t, 32)
ALLOC_USCALAR(uint64_t, uint16_t, 64)
ALLOC_USCALAR(size_t, size_t, (8*sizeof(size_t)))

PRINT_USCALAR(bool, bool, "%d")
PRINT_USCALAR(uint, unsigned int, "%u")
PRINT_USCALAR(uint8_t, uint8_t, "%" PRIu8)
PRINT_USCALAR(uint16_t, uint16_t, "%" PRIu16)
PRINT_USCALAR(uint32_t, uint32_t, "%" PRIu32)
PRINT_USCALAR(uint64_t, uint64_t, "%" PRIu64)
PRINT_USCALAR(size_t, size_t, "%zu")

HASH_USCALAR(bool, bool)
HASH_USCALAR(uint, unsigned int)
HASH_USCALAR(uint8_t, uint8_t)
HASH_USCALAR(uint16_t, uint16_t)
HASH_USCALAR(uint32_t, uint32_t)
HASH_USCALAR(uint64_t, uint64_t)
HASH_USCALAR(size_t, size_t)

SHRINK_USCALAR(uint, unsigned int)
SHRINK_USCALAR(uint8_t, uint8_t)
SHRINK_USCALAR(uint16_t, uint16_t)
SHRINK_USCALAR(uint32_t, uint32_t)
SHRINK_USCALAR(uint64_t, uint64_t)
SHRINK_USCALAR(size_t, size_t)

#define USCALAR_ROW(NAME)                                              \
    { .key = THEFT_BUILTIN_ ## NAME,                                   \
          .value = { .alloc = NAME ## _alloc,                          \
                     .free = theft_generic_free,                       \
                     .hash = NAME ## _hash,                            \
                     .print = NAME ## _print,                          \
                     .shrink = NAME ## _shrink,                        \
        },                                                             \
    }

static struct type_info_row rows[] = {
    { .key = THEFT_BUILTIN_bool,
      .value = { .alloc = bool_alloc,
                 .free = theft_generic_free,
                 .hash = bool_hash,
                 .print = bool_print,
        },
    },
    USCALAR_ROW(uint),
    USCALAR_ROW(uint8_t),
    USCALAR_ROW(uint16_t),
    USCALAR_ROW(uint32_t),
    USCALAR_ROW(uint64_t),
    USCALAR_ROW(size_t),
};

void theft_get_builtin_type_info(enum theft_builtin_type_info type,
    struct theft_type_info *info) {
    for (size_t i = 0; i < sizeof(rows)/sizeof(rows[i]); i++) {
        struct type_info_row *row = &rows[i];
        if (row->key == type) {
            memcpy(info, &row->value, sizeof(row->value));
            return;
        }
    }
    assert(false);
}

