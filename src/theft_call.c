#include "theft_call.h"

#include "theft_bloom.h"
#include <assert.h>

/* Actually call the property function. Its number of arguments is not
 * constrained by the typedef, but will be defined at the call site
 * here. (If info->arity is wrong, it will probably crash.) */
enum theft_trial_res
theft_call(struct theft_propfun_info *info, void **args) {
    assert(info);
    enum theft_trial_res res = THEFT_TRIAL_ERROR;
    switch (info->arity) {
    case 1:
        res = info->fun(args[0]);
        break;
    case 2:
        res = info->fun(args[0], args[1]);
        break;
    case 3:
        res = info->fun(args[0], args[1], args[2]);
        break;
    case 4:
        res = info->fun(args[0], args[1], args[2], args[3]);
        break;
    case 5:
        res = info->fun(args[0], args[1], args[2], args[3], args[4]);
        break;
    case 6:
        res = info->fun(args[0], args[1], args[2], args[3], args[4],
            args[5]);
        break;
    case 7:
        res = info->fun(args[0], args[1], args[2], args[3], args[4],
            args[5], args[6]);
        break;
    case 8:
        res = info->fun(args[0], args[1], args[2], args[3], args[4],
            args[5], args[6], args[7]);
        break;
    case 9:
        res = info->fun(args[0], args[1], args[2], args[3], args[4],
            args[5], args[6], args[7], args[8]);
        break;
    case 10:
        res = info->fun(args[0], args[1], args[2], args[3], args[4],
            args[5], args[6], args[7], args[8], args[9]);
        break;
    /* ... */
    default:
        return THEFT_TRIAL_ERROR;
    }
    return res;
}

/* Populate a buffer with hashes of all the arguments. */
static void
get_arg_hash_buffer(theft_hash *buffer,
        struct theft_propfun_info *info, void **args, void *env) {
    for (int i = 0; i < info->arity; i++) {
        buffer[i] = info->type_info[i]->hash(args[i], env);
    }    
}

/* Check if this combination of argument instances has been called. */
bool theft_call_check_called(struct theft *t,
        struct theft_propfun_info *info, void **args, void *env) {
    theft_hash buffer[THEFT_MAX_ARITY];
    get_arg_hash_buffer(buffer, info, args, env);
    return theft_bloom_check(t->bloom, (uint8_t *)buffer,
        info->arity * sizeof(theft_hash));
}

/* Mark the tuple of argument instances as called in the bloom filter. */
void theft_call_mark_called(struct theft *t,
        struct theft_propfun_info *info, void **args, void *env) {
    theft_hash buffer[THEFT_MAX_ARITY];
    get_arg_hash_buffer(buffer, info, args, env);
    theft_bloom_mark(t->bloom, (uint8_t *)buffer,
        info->arity * sizeof(theft_hash));
}

