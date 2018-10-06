#include "theft.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

struct type_info_row {
    enum theft_builtin_type_info key;
    struct theft_type_info value;
};

static enum theft_alloc_res
bool_alloc(struct theft *t, void *env, void **instance) {
    (void)env;
    bool *res = malloc(sizeof(*res));
    if (res == NULL) { return THEFT_ALLOC_ERROR; }
    *res = (bool)theft_random_bits(t, 1);
    *instance = res;
    return THEFT_ALLOC_OK;
}

#define BITS_USE_SPECIAL (3)

#define ALLOC_USCALAR(NAME, TYPE, BITS, ...)                           \
static enum theft_alloc_res                                            \
NAME ## _alloc(struct theft *t, void *env, void **instance) {          \
    TYPE *res = malloc(sizeof(*res));                                  \
    if (res == NULL) { return THEFT_ALLOC_ERROR; }                     \
    if (((1LU << BITS_USE_SPECIAL) - 1 ) ==                            \
        theft_random_bits(t, BITS_USE_SPECIAL)) {                      \
        const TYPE special[] = { __VA_ARGS__ };                        \
        size_t idx = theft_random_bits(t, 8)                           \
          % (sizeof(special)/sizeof(special[0]));                      \
        *res = special[idx];                                           \
    } else {                                                           \
        *res = (TYPE)theft_random_bits(t, BITS);                       \
    }                                                                  \
    if (env != NULL) {                                                 \
        TYPE limit = *(TYPE *)env;                                     \
        assert(limit != 0);                                            \
        (*res) %= limit;                                               \
    }                                                                  \
    *instance = res;                                                   \
    return THEFT_ALLOC_OK;                                             \
}

#define ALLOC_SSCALAR(NAME, TYPE, BITS, ...)                           \
static enum theft_alloc_res                                            \
NAME ## _alloc(struct theft *t, void *env, void **instance) {          \
    TYPE *res = malloc(sizeof(*res));                                  \
    if (res == NULL) { return THEFT_ALLOC_ERROR; }                     \
    if (((1LU << BITS_USE_SPECIAL) - 1 ) ==                            \
        theft_random_bits(t, BITS_USE_SPECIAL)) {                      \
        const TYPE special[] = { __VA_ARGS__ };                        \
        size_t idx = theft_random_bits(t, 8)                           \
          % (sizeof(special)/sizeof(special[0]));                      \
        *res = special[idx];                                           \
    } else {                                                           \
        *res = (TYPE)theft_random_bits(t, BITS);                       \
    }                                                                  \
    if (env != NULL) {                                                 \
        TYPE limit = *(TYPE *)env;                                     \
        assert(limit > 0); /* -limit <= res < limit */                 \
        if (*res < (-limit)) {                                         \
            *res %= (-limit);                                          \
        } else if (*res >= limit) {                                    \
            (*res) %= limit;                                           \
        }                                                              \
    }                                                                  \
    *instance = res;                                                   \
    return THEFT_ALLOC_OK;                                             \
}

#define ALLOC_FSCALAR(NAME, TYPE, MOD, BITS, ...)                      \
static enum theft_alloc_res                                            \
NAME ## _alloc(struct theft *t, void *env, void **instance) {          \
    TYPE *res = malloc(sizeof(*res));                                  \
    if (res == NULL) { return THEFT_ALLOC_ERROR; }                     \
    if (((1LU << BITS_USE_SPECIAL) - 1 ) ==                            \
        theft_random_bits(t, BITS_USE_SPECIAL)) {                      \
        const TYPE special[] = { __VA_ARGS__ };                        \
        size_t idx = theft_random_bits(t, 8)                           \
          % (sizeof(special)/sizeof(special[0]));                      \
        *res = special[idx];                                           \
    } else {                                                           \
        *res = (TYPE)theft_random_bits(t, BITS);                       \
    }                                                                  \
    if (env != NULL) {                                                 \
        TYPE limit = *(TYPE *)env;                                     \
        assert(limit > 0); /* -limit <= res < limit */                 \
        if (*res < (-limit)) {                                         \
            *res = MOD(*res, -limit);                                  \
        } else {                                                       \
            *res = MOD(*res, limit);                                   \
        }                                                              \
    }                                                                  \
    *instance = res;                                                   \
    return THEFT_ALLOC_OK;                                             \
}

#define PRINT_SCALAR(NAME, TYPE, FORMAT)                               \
    static void NAME ## _print(FILE *f,                                \
            const void *instance, void *env) {                         \
        (void)env;                                                     \
        fprintf(f, FORMAT, *(TYPE *)instance);                         \
    }

ALLOC_USCALAR(uint, unsigned int, 8*sizeof(unsigned int),
    0, 1, 2, 3, 4, 5, 6, 7,
    63, 64, 127, 128, 129, 255, UINT_MAX - 1, UINT_MAX)

ALLOC_USCALAR(uint8_t, uint8_t, 8*sizeof(uint8_t),
    0, 1, 2, 3, 4, 5, 6, 7,
    63, 64, 65, 127, 128, 129, 254, 255)

ALLOC_USCALAR(uint16_t, uint16_t, 8*sizeof(uint16_t),
    0, 1, 2, 3, 4, 5, 6, 255,
    256, 1024, 4096, 16384, 32768, 32769, 65534, 65535)

ALLOC_USCALAR(uint32_t, uint32_t, 8*sizeof(uint32_t),
    0, 1, 2, 3, 4, 5, 6, 255,
    (1LU << 8), (1LU << 8) + 1, (1LU << 16) - 1, (1LU << 16),
    (1LU << 16) + 1, (1LU << 19), (1LU << 22), (1LLU << 32) - 1)

ALLOC_USCALAR(uint64_t, uint64_t, 8*sizeof(uint64_t),
    0, 1, 2, 3, 4, 5, 6, 255,
    (1LLU << 8), (1LLU << 16), (1LLU << 32), (1LLU << 32) + 1,
    (1LLU << 53), (1LLU << 53) + 1, (uint64_t)-2, (uint64_t)-1)

ALLOC_USCALAR(size_t, size_t, 8*sizeof(size_t),
    0, 1, 2, 3, 4, 5, 6, 255,
    256, (size_t)-2, (size_t)-1)


ALLOC_SSCALAR(int, unsigned int, 8*sizeof(int),
    0, 1, 2, 3, -1, -2, -3, -4,
    INT_MIN + 1, INT_MIN, INT_MAX - 1, INT_MAX)

ALLOC_SSCALAR(int8_t, int8_t, 8*sizeof(int8_t),
    0, 1, 2, 3, -1, -2, -3, -4,
    63, 64, 65, 127,
    (int8_t)128, (int8_t)129, (int8_t)254, (int8_t)255)

ALLOC_SSCALAR(int16_t, int16_t, 8*sizeof(int16_t),
    0, 1, 2, 3, 4, 5, 6, 255,
    256, 1024, 4096, 16384,
    (int16_t)32768, (int16_t)32769, (int16_t)65534, (int16_t)65535)

ALLOC_SSCALAR(int32_t, int32_t, 8*sizeof(int32_t),
    0, 1, 2, 3, 4, 5, 6, 255,
    (1LU << 8), (1LU << 8) + 1, (1LU << 16) - 1, (1LU << 16),
    (int32_t)(1LU << 16) + 1, (int32_t)(1LU << 19),
    (int32_t)(1LU << 22), (int32_t)(1LLU << 32) - 1)

ALLOC_SSCALAR(int64_t, int64_t, 8*sizeof(int64_t),
    0, 1, 2, 3, 4, 5, 6, 255,
    (1LLU << 8), (1LLU << 16), (1LLU << 32), (1LLU << 32) + 1,
    (1LLU << 53), (1LLU << 53) + 1, (int64_t)-2, (int64_t)-1)

PRINT_SCALAR(bool, bool, "%d")
PRINT_SCALAR(uint, unsigned int, "%u")
PRINT_SCALAR(uint8_t, uint8_t, "%" PRIu8)
PRINT_SCALAR(uint16_t, uint16_t, "%" PRIu16)
PRINT_SCALAR(uint32_t, uint32_t, "%" PRIu32)
PRINT_SCALAR(uint64_t, uint64_t, "%" PRIu64)
PRINT_SCALAR(size_t, size_t, "%zu")

PRINT_SCALAR(int, int, "%d")
PRINT_SCALAR(int8_t, int8_t, "%" PRId8)
PRINT_SCALAR(int16_t, int16_t, "%" PRId16)
PRINT_SCALAR(int32_t, int32_t, "%" PRId32)
PRINT_SCALAR(int64_t, int64_t, "%" PRId64)

#if THEFT_USE_FLOATING_POINT
#include <math.h>
#include <float.h>
ALLOC_FSCALAR(float, float, fmodf, 8*sizeof(float),
    0, 1, -1, NAN,
    INFINITY, -INFINITY, FLT_MIN, FLT_MAX)
ALLOC_FSCALAR(double, double, fmod, 8*sizeof(double),
    0, 1, -1, NAN,
    NAN, INFINITY, -INFINITY, DBL_MIN, DBL_MAX)

static void float_print(FILE *f, const void *instance, void *env) {
    (void)env;
    float fl = *(float *)instance;
    uint32_t u32 = (uint32_t)fl;
    fprintf(f, "%g (0x%08" PRIx32 ")", fl, u32);
}

static void double_print(FILE *f, const void *instance, void *env) {
    (void)env;
    double d = *(double *)instance;
    uint64_t u64 = (uint64_t)d;
    fprintf(f, "%g (0x%016" PRIx64 ")", d, u64);
}

#endif

#define SCALAR_ROW(NAME)                                               \
    {                                                                  \
        .key = THEFT_BUILTIN_ ## NAME,                                 \
          .value = {                                                   \
            .alloc = NAME ## _alloc,                                   \
            .free = theft_generic_free_cb,                             \
            .print = NAME ## _print,                                   \
            .autoshrink_config = {                                     \
                .enable = true,                                        \
            },                                                         \
        },                                                             \
    }

#define DEF_BYTE_ARRAY_CEIL 8
static enum theft_alloc_res
char_ARRAY_alloc(struct theft *t, void *env, void **instance) {
    (void)env;
    size_t ceil = DEF_BYTE_ARRAY_CEIL;
    size_t size = 0;
    size_t *max_length = NULL;
    if (env != NULL) {
        max_length = (size_t *)env;
        assert(*max_length > 0);
    }

    char *res = malloc(ceil * sizeof(char));
    if (res == NULL) { return THEFT_ALLOC_ERROR; }
    while (true) {
        if (max_length != NULL && size + 1 == *max_length) {
            res[size] = 0;
            break;
        } else if (size == ceil) {
            const size_t nceil = 2 * ceil;
            char *nres = realloc(res, nceil * sizeof(char));
            if (nres == NULL) {
                free(res);
                return THEFT_ALLOC_ERROR;
            }
            res = nres;
            ceil = nceil;
        }
        char byte = theft_random_bits(t, 8);
        res[size] = byte;
        if (byte == 0x00) {
            break;
        }
        size++;
    }

    *instance = res;
    return THEFT_ALLOC_OK;
}

static void hexdump(FILE *f, const uint8_t *raw, size_t size) {
    for (size_t row_i = 0; row_i < size; row_i += 16) {
        size_t rem = (size - row_i > 16 ? 16 : size - row_i);
        fprintf(f, "%04zx: ", row_i);
        for (size_t i = 0; i < rem; i++) {
            fprintf(f, "%02x ", raw[row_i + i]);
        }

        for (size_t ii = rem; ii < 16; ++ii)
            fprintf(f, "   ");  /* add padding */

        for (size_t i = 0; i < rem; i++) {
            char c = ((const char *)raw)[i];
            fprintf(f, "%c", (isprint(c) ? c : '.'));
        }
        fprintf(f, "\n");
    }
}

static void char_ARRAY_print(FILE *f, const void *instance, void *env) {
    (void)env;
    const char *s = (const char *)instance;
    size_t len = strlen(s);
    hexdump(f, (const uint8_t *)s, len);
}

static struct type_info_row rows[] = {
    {
        .key = THEFT_BUILTIN_bool,
          .value = {
            .alloc = bool_alloc,
            .free = theft_generic_free_cb,
            .print = bool_print,
            .autoshrink_config = {
                .enable = true,
            },
        },
    },
    SCALAR_ROW(uint),
    SCALAR_ROW(uint8_t),
    SCALAR_ROW(uint16_t),
    SCALAR_ROW(uint32_t),
    SCALAR_ROW(uint64_t),
    SCALAR_ROW(size_t),

    SCALAR_ROW(int),
    SCALAR_ROW(int8_t),
    SCALAR_ROW(int16_t),
    SCALAR_ROW(int32_t),
    SCALAR_ROW(int64_t),

#if THEFT_USE_FLOATING_POINT
    SCALAR_ROW(float),
    SCALAR_ROW(double),
#endif

    {
        .key = THEFT_BUILTIN_char_ARRAY,
          .value = {
            .alloc = char_ARRAY_alloc,
            .free = theft_generic_free_cb,
            .print = char_ARRAY_print,
            .autoshrink_config = {
                .enable = true,
            },
        },
    },
    /* This is actually the same implementation, but
     * the user should cast it differently. */
    {
        .key = THEFT_BUILTIN_uint8_t_ARRAY,
          .value = {
            .alloc = char_ARRAY_alloc,
            .free = theft_generic_free_cb,
            .print = char_ARRAY_print,
            .autoshrink_config = {
                .enable = true,
            },
        },
    },
};

const struct theft_type_info *
theft_get_builtin_type_info(enum theft_builtin_type_info type) {
    for (size_t i = 0; i < sizeof(rows)/sizeof(rows[0]); i++) {
        const struct type_info_row *row = &rows[i];
        if (row->key == type) {
            return &row->value;
        }
    }
    assert(false);
    return NULL;
}

void
theft_copy_builtin_type_info(enum theft_builtin_type_info type,
    struct theft_type_info *info) {
    const struct theft_type_info *builtin = theft_get_builtin_type_info(type);
    memcpy(info, builtin, sizeof(*builtin));
}
