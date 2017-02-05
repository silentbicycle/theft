#ifndef THEFT_AUX_H
#define THEFT_AUX_H

#include "theft.h"

#ifndef THEFT_USE_FLOATING_POINT
#define THEFT_USE_FLOATING_POINT 1
#endif

enum theft_builtin_type_info {
    THEFT_BUILTIN_bool,

    /* Built-in unsigned types.
     * If env is non-NULL, it will be cast to
     * a pointer of this type and dereferenced
     * for a limit. */
    //THEFT_BUILTIN_uint,  // platform-specific
    THEFT_BUILTIN_uint8_t,
    THEFT_BUILTIN_uint16_t,
    THEFT_BUILTIN_uint32_t,
    THEFT_BUILTIN_uint64_t,
    THEFT_BUILTIN_size_t,

    /* Built-in signed types.
     * If env is non-NULL, it will be cast to
     * a pointer of this type and dereferenced
     * for a +/- limit (i.e., a pointer to an
     * int16_t of 100 will lead to generated
     * values from -100 to 100, inclusive). */
    //THEFT_BUILTIN_int8_t,
    //THEFT_BUILTIN_int16_t,
    //THEFT_BUILTIN_int32_t,
    //THEFT_BUILTIN_int64_t,

#if THEFT_USE_FLOATING_POINT
    /* Built-in floating point types.
     * If env is non-NULL, it will be cast to a
     * pointer of this type and dereferenced for
     * a +/- limit. */
    //THEFT_BUILTIN_float,
    //THEFT_BUILTIN_double,
#endif

    /* Built-in array types.
     * If env is non-NULL, it will be cast to a
     * `size_t *` and deferenced for a max length. */
    //THEFT_BUILTIN_char_ARRAY
    //THEFT_BUILTIN_uint8_t_ARRAY
};

/* Copy built-in type_info callbacks for TYPE into INFO.
 * See the comments for each type above for details. */
void theft_get_builtin_type_info(enum theft_builtin_type_info type,
    struct theft_type_info *info);

/* Generic free callback: just call free(instance). */
void theft_generic_free_cb(void *instance, void *env);

/* Generic hash callback: ... (FIXME aligrment?). */
void theft_generic_hash_cb(void *instance, void *env);

#endif
