#ifndef THEFT_AUX_H
#define THEFT_AUX_H

#include "theft.h"

/* FIXME: move this into main theft / theft_types headers */

#ifndef THEFT_USE_FLOATING_POINT
#define THEFT_USE_FLOATING_POINT 1
#endif

enum theft_builtin_type_info {
    THEFT_BUILTIN_bool,

    /* Built-in unsigned types.
     * If env is non-NULL, it will be cast to
     * a pointer of this type and dereferenced
     * for a limit. */
    THEFT_BUILTIN_uint,  // platform-specific
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
    THEFT_BUILTIN_int,
    THEFT_BUILTIN_int8_t,
    THEFT_BUILTIN_int16_t,
    THEFT_BUILTIN_int32_t,
    THEFT_BUILTIN_int64_t,

#if THEFT_USE_FLOATING_POINT
    /* Built-in floating point types.
     * If env is non-NULL, it will be cast to a
     * pointer of this type and dereferenced for
     * a +/- limit. */
    THEFT_BUILTIN_float,
    THEFT_BUILTIN_double,
#endif

    /* Built-in array types.
     * If env is non-NULL, it will be cast to a
     * `size_t *` and deferenced for a max length. */
    THEFT_BUILTIN_char_ARRAY,
    THEFT_BUILTIN_uint8_t_ARRAY,
    //THEFT_BUILTIN_UTF_8_ARRAY,
};

/* Get a connst pointer to built-in type_info callbacks for
 * TYPE. See the comments for each type above for details.
 *
 * NOTE: All built-ins have autoshrink enabled. */
const struct theft_type_info *
theft_get_builtin_type_info(enum theft_builtin_type_info type);

/* Copy a built-in type info into INFO, so its fields can be
 * modified (e.g. setting a limit in info->env). */
void
theft_copy_builtin_type_info(enum theft_builtin_type_info type,
    struct theft_type_info *info);

/* Generic free callback: just call free(instance). */
void theft_generic_free_cb(void *instance, void *env);

/* Get a seed based on the hash of the current timestamp. */
theft_seed theft_seed_of_time(void);

struct theft_print_trial_result_env {
    FILE *f;                    /* 0 -> default of stdout */
    const uint8_t max_column;   /* 0 -> default of 72 */
    uint8_t column;
    size_t scale_pass;
    size_t scale_skip;
    size_t scale_dup;
    size_t consec_pass;
    size_t consec_skip;
    size_t consec_dup;
};

/* Print a trial result. */
void theft_print_trial_result(
    struct theft_print_trial_result_env *print_env,
    const struct theft_hook_trial_post_info *info);

/* Print a run report. */
void theft_print_run_post_info(FILE *f,
    const struct theft_hook_run_post_info *info);

/* A run-post hook that just calls theft_print_run_post_info and returns
 * THEFT_HOOK_RUN_POST_CONTINUE. */
enum theft_hook_run_post_res
theft_hook_run_post_print_info(const struct theft_hook_run_post_info *info, void *env);

#endif
