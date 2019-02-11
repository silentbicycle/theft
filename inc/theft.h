#ifndef THEFT_H
#define THEFT_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* Version 0.4.5 */
#define THEFT_VERSION_MAJOR 0
#define THEFT_VERSION_MINOR 4
#define THEFT_VERSION_PATCH 5

#include "theft_types.h"

/* Run a series of randomized trials of a property function.
 *
 * Configuration is specified in CFG; many fields are optional.
 * See the type definition in `theft_types.h`. */
enum theft_run_res
theft_run(const struct theft_run_config *cfg);

/* Generate the instance based on a given seed, print it to F,
 * and then free it. (If print or free callbacks are NULL,
 * they will be skipped.) */
enum theft_generate_res
theft_generate(FILE *f, theft_seed seed,
    const struct theft_type_info *info, void *hook_env);


/***********************
 * Getting random bits *
 ***********************/

/* Get a random 64-bit integer from the test runner's PRNG.
 *
 * DEPRECATED: This is equivalent to `theft_random_bits(t, 64)`, but
 * theft works better when the caller only requests as many bits as
 * it needs. This will be removed in a future release! */
uint64_t theft_random(struct theft *t);

/* Get BITS random bits from the test runner's PRNG, which will be
 * returned as a little-endian uint64_t. At most 64 bits can be
 * retrieved at once -- requesting more is a checked error.
 *
 * For more than 64 bits, use theft_random_bits_bulk. */
uint64_t theft_random_bits(struct theft *t, uint8_t bits);

/* Get BITS random bits, in bulk, and put them in BUF.
 * BUF is assumed to be large enough, and will be zeroed
 * before any bits are copied to it. Bits will be copied
 * little-endian. */
void theft_random_bits_bulk(struct theft *t, uint32_t bits, uint64_t *buf);

#if THEFT_USE_FLOATING_POINT
/* Get a random double from the test runner's PRNG. */
double theft_random_double(struct theft *t);

/* Get a random uint64_t less than CEIL.
 * For example, `theft_random_choice(t, 5)` will return
 * approximately evenly distributed values from [0, 1, 2, 3, 4]. */
uint64_t theft_random_choice(struct theft *t, uint64_t ceil);
#endif


/***********
 * Hashing *
 ***********/

/* Hash a buffer in one pass. (Wraps the below functions.) */
theft_hash theft_hash_onepass(const uint8_t *data, size_t bytes);

/* Initialize/reset a hasher for incremental hashing. */
void theft_hash_init(struct theft_hasher *h);

/* Sink more data into an incremental hash. */
void theft_hash_sink(struct theft_hasher *h,
    const uint8_t *data, size_t bytes);

/* Finish hashing and get the result.
 * (This also resets the internal hasher state.) */
theft_hash theft_hash_done(struct theft_hasher *h);


/*********
 * Hooks *
 *********/

/* Print a trial result in the default format.
 *
 * To use this, add a `struct theft_print_trial_result_env` to the env
 * in the `struct theft_run_config`, and call `theft_print_trial_result`
 * with it from inside the `trial_post` hook.
 *
 * When the default `theft_hook_trial_post_print_result` hook is used,
 * the env is allocated and freed internally.
 *
 * Unless a custom output max_column width is wanted, all of these
 * fields can just be initialized to 0. */
#define THEFT_PRINT_TRIAL_RESULT_ENV_TAG 0xe7a6
struct theft_print_trial_result_env {
    uint16_t tag;               /* used for internal validation */
    const uint8_t max_column;   /* 0 -> default of 72 */
    uint8_t column;
    size_t scale_pass;
    size_t scale_skip;
    size_t scale_dup;
    size_t consec_pass;
    size_t consec_skip;
    size_t consec_dup;
};
void theft_print_trial_result(
    struct theft_print_trial_result_env *print_env,
    const struct theft_hook_trial_post_info *info);

/* Print a property counter-example that caused a failing trial.
 * This is the default counterexample hook. */
enum theft_hook_counterexample_res
theft_print_counterexample(const struct theft_hook_counterexample_info *info,
    void *env);

/* Print a standard pre-run report. */
void theft_print_run_pre_info(FILE *f,
    const struct theft_hook_run_pre_info *info);

/* Print a standard post-run report. */
void theft_print_run_post_info(FILE *f,
    const struct theft_hook_run_post_info *info);

/* A run-pre hook that just calls theft_print_run_pre_info and returns
 * THEFT_HOOK_RUN_PRE_CONTINUE. */
enum theft_hook_run_pre_res
theft_hook_run_pre_print_info(const struct theft_hook_run_pre_info *info, void *env);

/* Halt trials after the first failure. */
enum theft_hook_trial_pre_res
theft_hook_first_fail_halt(const struct theft_hook_trial_pre_info *info, void *env);

/* The default trial-post hook, which just calls theft_print_trial_result,
 * with an internally allocated `struct theft_print_trial_result_env`. */
enum theft_hook_trial_post_res
theft_hook_trial_post_print_result(const struct theft_hook_trial_post_info *info,
    void *env);

/* A run-post hook that just calls theft_print_run_post_info and returns
 * THEFT_HOOK_RUN_POST_CONTINUE. */
enum theft_hook_run_post_res
theft_hook_run_post_print_info(const struct theft_hook_run_post_info *info, void *env);

/* Get the hook environment pointer.
 * This is the contents of theft_run_config.hooks.env. */
void *theft_hook_get_env(struct theft *t);


/***************************
 * Other utility functions *
 ***************************/

/* Change T's output stream handle to OUT. (Default: stdout.) */
void theft_set_output_stream(struct theft *t, FILE *out);

/* Get a seed based on the hash of the current timestamp. */
theft_seed theft_seed_of_time(void);

/* Generic free callback: just call free(instance). */
void theft_generic_free_cb(void *instance, void *env);

/* Return a string name of a trial result. */
const char *theft_trial_res_str(enum theft_trial_res res);

/* Return a string name of a run result. */
const char *theft_run_res_str(enum theft_run_res res);

/***********************
 * Built-in generators *
 ***********************/

enum theft_builtin_type_info {
    THEFT_BUILTIN_bool,

    /* Built-in unsigned types.
     *
     * If env is non-NULL, it will be cast to a pointer to this type and
     * dereferenced for a limit.
     *
     * For example, if the theft_type_info struct's env field is set
     * like this:
     *
     *     uint8_t limit = 64;
     *     struct theft_type_info info;
     *     theft_copy_builtin_type_info(THEFT_BUILTIN_uint8_t, &info);
     *     info.env = &limit;
     *
     * then the generator will produce uint8_t values 0 <= x < 64. */
    THEFT_BUILTIN_uint,  // platform-specific
    THEFT_BUILTIN_uint8_t,
    THEFT_BUILTIN_uint16_t,
    THEFT_BUILTIN_uint32_t,
    THEFT_BUILTIN_uint64_t,
    THEFT_BUILTIN_size_t,

    /* Built-in signed types.
     *
     * If env is non-NULL, it will be cast to a pointer to this type and
     * dereferenced for a +/- limit.
     *
     * For example, if if the theft_type_info struct's env field is set
     * like this:
     *
     *     int16_t limit = 1000;  // limit must be positive
     *     struct theft_type_info info;
     *     theft_copy_builtin_type_info(THEFT_BUILTIN_int16_t, &info);
     *     info.env = &limit;
     *
     * then the generator will produce uint8_t values -1000 <= x < 1000. */
    THEFT_BUILTIN_int,
    THEFT_BUILTIN_int8_t,
    THEFT_BUILTIN_int16_t,
    THEFT_BUILTIN_int32_t,
    THEFT_BUILTIN_int64_t,

#if THEFT_USE_FLOATING_POINT
    /* Built-in floating point types.
     * If env is non-NULL, it will be cast to a pointer of this type and
     * dereferenced for a +/- limit. */
    THEFT_BUILTIN_float,
    THEFT_BUILTIN_double,
#endif

    /* Built-in array types.
     * If env is non-NULL, it will be cast to a `size_t *` and
     * deferenced for a max length. */
    THEFT_BUILTIN_char_ARRAY,
    THEFT_BUILTIN_uint8_t_ARRAY,
};

/* Get a const pointer to built-in type_info callbacks for
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

#endif
