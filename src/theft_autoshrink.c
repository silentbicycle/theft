#include "theft_autoshrink_internal.h"

#include "theft_random.h"

#include <string.h>
#include <assert.h>

#define ENABLE_LOG 0
#if ENABLE_LOG
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

#define GET_DEF(X, DEF) (X ? X : DEF)

static autoshrink_prng_fun *get_prng(struct theft *t, struct theft_autoshrink_env *env);

bool theft_autoshrink_wrap(struct theft *t,
        struct theft_type_info *type_info, struct theft_type_info *wrapper) {
    (void)t;
    if (type_info->alloc == NULL || type_info->shrink != NULL) {
        return false;           /* API misuse */
    }

    struct theft_autoshrink_env *env = calloc(1, sizeof(*env));
    if (env == NULL) {
        return false;
    }

    *env = (struct theft_autoshrink_env) {
        .tag = AUTOSHRINK_ENV_TAG,
        .pool_size = type_info->autoshrink_config.pool_size,
        .print_mode = type_info->autoshrink_config.print_mode,
        .max_failed_shrinks = type_info->autoshrink_config.max_failed_shrinks,
    };
    memcpy(&env->user_type_info, type_info, sizeof(*type_info));

    *wrapper = (struct theft_type_info) {
        .alloc = autoshrink_alloc,
        .free = autoshrink_free,
        .hash = autoshrink_hash,
        .shrink = autoshrink_shrink,
        .print = autoshrink_print,
        .autoshrink_config = {
            .enable = true,
        },
        .env = env,
    };

    return true;
}

uint64_t
theft_autoshrink_bit_pool_random(struct theft_autoshrink_bit_pool *pool,
        uint8_t bit_count, bool save_request) {
    assert(pool);
    assert(bit_count <= 64);

    /* Only return as many bits as the pool contains. After reaching the
     * end of the pool, just return 0 bits forever and stop tracking
     * requests. */
    if (pool->consumed == pool->size) {
        return 0;
    } else if (pool->consumed + bit_count >= pool->size) {
        bit_count = pool->size - pool->consumed;
    }

    if (save_request) {
        if (!append_request(pool, bit_count)) {
            assert(false); // memory fail
        }
    }

    uint64_t res = 0;
    size_t offset = pool->consumed / 8;
    uint8_t bit = 1LU << (pool->consumed & 0x07);
    const uint8_t *bits = pool->bits;

    for (uint8_t i = 0; i < bit_count; i++) {
        res |= (bits[offset] & bit) ? (1LLU << i) : 0;
        if (bit == 0x80) {
            bit = 0x01;
            offset++;
        } else {
            bit <<= 1;
        }
    }

    pool->consumed += bit_count;
    return res;
}

static size_t get_aligned_size(size_t size, uint8_t alignment) {
    if ((size % alignment) != 0) {
        size += alignment - (size % alignment);
    }
    return size;
}

static struct theft_autoshrink_bit_pool *
alloc_bit_pool(size_t size, size_t request_ceil) {
    uint8_t *bits = NULL;
    uint32_t *requests = NULL;
    struct theft_autoshrink_bit_pool *res = NULL;

    size_t alloc_size = get_aligned_size(size, 32);
    assert((alloc_size % 32) == 0);

    /* Ensure that the allocation size is aligned to 32 bits, so we can
     * work in 32-bit steps later on. */
    LOG("Allocating alloc_size %zd => %zd bytes\n",
        alloc_size, (alloc_size/32) * sizeof(uint32_t));
    uint32_t *aligned_bits = calloc(alloc_size/32, sizeof(uint32_t));
    bits = (uint8_t *)aligned_bits;
    if (bits == NULL) { goto fail; }

    res = calloc(1, sizeof(*res));
    if (res == NULL) { goto fail; }

    requests = calloc(request_ceil, sizeof(*requests));
    if (requests == NULL) { goto fail; }

    *res = (struct theft_autoshrink_bit_pool) {
        .tag = AUTOSHRINK_BIT_POOL_TAG,
        .bits = bits,
        .size = size,
        .request_count = 0,
        .request_ceil = request_ceil,
        .requests = requests,
    };
    return res;

fail:
    if (bits) { free(bits); }
    if (res) { free(res); }
    if (requests) { free(requests); }
    return NULL;
}

static struct theft_autoshrink_bit_pool *
init_bit_pool(struct theft *t, size_t size,
              theft_seed seed, size_t request_ceil) {
    assert(size);
    struct theft_autoshrink_bit_pool *res = NULL;
    res = alloc_bit_pool(size, request_ceil);
    if (res == NULL) {
        return NULL;
    }

    theft_random_set_seed(t, seed);
    for (size_t i = 0; i < size / 8; i++) {
        res->bits[i] = theft_random_bits(t, 8);
    }
    return res;
}

void theft_autoshrink_free_bit_pool(struct theft *t,
        struct theft_autoshrink_bit_pool *pool) {
    if (t) {
        assert(t->bit_pool == NULL);  // don't free while still in use
    }
    assert(pool->instance == NULL);
    free(pool->bits);
    free(pool->requests);
    free(pool);
}

void
theft_autoshrink_get_real_args(struct theft_run_info *run_info,
        void **dst, void **src) {
    for (size_t i = 0; i < run_info->arity; i++) {
        const struct theft_type_info *ti = run_info->type_info[i];
        if (ti->autoshrink_config.enable) {
            struct theft_autoshrink_bit_pool *bit_pool =
              (struct theft_autoshrink_bit_pool *)src[i];
            assert(bit_pool);
            assert(bit_pool->tag == AUTOSHRINK_BIT_POOL_TAG);
            dst[i] = bit_pool->instance;
        } else {
            dst[i] = src[i];
        }
    }
}

#define CHECK_ENV_CAST(NAME, VENV)                                    \
    struct theft_autoshrink_env *NAME =                               \
      (struct theft_autoshrink_env *)VENV;                            \
    assert(NAME->tag == AUTOSHRINK_ENV_TAG)

#define CHECK_BIT_POOL_CAST(NAME, INSTANCE)                           \
    struct theft_autoshrink_bit_pool *NAME =                          \
      (struct theft_autoshrink_bit_pool *)INSTANCE;                   \
    assert(NAME->tag == AUTOSHRINK_BIT_POOL_TAG)

static enum theft_alloc_res
alloc_from_bit_pool(struct theft *t, struct theft_autoshrink_env *env,
        struct theft_autoshrink_bit_pool *bit_pool, void **output) {
    enum theft_alloc_res ares;
    theft_random_inject_autoshrink_bit_pool(t, bit_pool);
    ares = env->user_type_info.alloc(t, env->user_type_info.env, output);
    theft_random_stop_using_bit_pool(t);
    return ares;
}

static enum theft_alloc_res
autoshrink_alloc(struct theft *t, void *venv, void **instance) {
    CHECK_ENV_CAST(env, venv);

    theft_seed seed = theft_random(t);

    size_t pool_size = GET_DEF(env->pool_size, DEF_POOL_SIZE);
    struct theft_autoshrink_bit_pool *pool =
      init_bit_pool(t, pool_size, seed, DEF_REQUESTS_CEIL);
    if (pool == NULL) {
        return THEFT_ALLOC_ERROR;
    }

    void *res = NULL;
    enum theft_alloc_res ares = alloc_from_bit_pool(t, env,
        pool, &res);
    if (ares != THEFT_ALLOC_OK) {
        theft_autoshrink_free_bit_pool(t, pool);
        return ares;
    }

    pool->instance = res;
    *instance = pool;
    return THEFT_ALLOC_OK;
}

static void
autoshrink_free(void *instance, void *venv) {
    CHECK_ENV_CAST(env, venv);
    CHECK_BIT_POOL_CAST(pool, instance);

    /* Call user's free callback on the instance, if set.
     * If not, just leak it. */
    if (env->user_type_info.free) {
        env->user_type_info.free(pool->instance,
            env->user_type_info.env);
    }

    pool->instance = NULL;

    theft_autoshrink_free_bit_pool(NULL, pool);

    (void)instance;
}

static theft_hash
autoshrink_hash(const void *instance, void *venv) {
    CHECK_ENV_CAST(env, venv);
    CHECK_BIT_POOL_CAST(pool, instance);

    /* If the user has a hash callback defined, use it on
     * the instance, otherwise hash the bit pool. */
    if (env->user_type_info.hash) {
        return env->user_type_info.hash(pool->instance,
            env->user_type_info.env);
    } else {
        /* Hash the consumed bits from the bit pool */
        struct theft_hasher h;
        theft_hash_init(&h);
        theft_hash_sink(&h, pool->bits, pool->consumed / 8);
        const uint8_t rem_bits = pool->consumed % 8;
        if (rem_bits > 0) {
            const uint8_t last_byte = pool->bits[pool->consumed / 8];
            const uint8_t mask = ((1U << rem_bits) - 1);
            uint8_t rem = last_byte & mask;
            theft_hash_sink(&h, &rem, 1);
        }
        return theft_hash_done(&h);
    }
}

static enum theft_shrink_res
autoshrink_shrink(struct theft *t, const void *instance, uint32_t tactic,
    void *venv, void **output) {
    CHECK_ENV_CAST(env, venv);
    const struct theft_autoshrink_bit_pool *orig
      = (struct theft_autoshrink_bit_pool *)instance;
    assert(orig->tag == AUTOSHRINK_BIT_POOL_TAG);

    return theft_autoshrink_shrink(t, orig, tactic, env, output);
}

enum theft_shrink_res
theft_autoshrink_shrink(struct theft *t,
                        const struct theft_autoshrink_bit_pool *orig,
                        uint32_t tactic,
                        struct theft_autoshrink_env *env, void **output) {
    if (tactic >= GET_DEF(env->max_failed_shrinks, DEF_MAX_FAILED_SHRINKS)) {
        return THEFT_SHRINK_NO_MORE_TACTICS;
    }

    /* Make a copy of the bit pool to shrink */
    struct theft_autoshrink_bit_pool *copy = alloc_bit_pool(orig->size, orig->request_ceil);
    if (copy == NULL) {
        return THEFT_SHRINK_ERROR;
    }
    size_t total_consumed = 0;
    for (size_t i = 0; i < orig->request_count; i++) {
        total_consumed += orig->requests[i];
    }
    assert(total_consumed == orig->consumed);

    LOG("========== BEFORE (tactic %u)\n", tactic);
    if (ENABLE_LOG) {
        theft_autoshrink_dump_bit_pool(stdout, orig->size,
            orig, THEFT_AUTOSHRINK_PRINT_ALL);
    }

    /* Alternate dropping requests from the pool and mutating individual
     * requests. Since tactic 0 will trigger dropping and successful
     * shrinks reset the tactic to 0, this means it will favor dropping
     * as long as it's effective.
     *
     * TODO: Some sort of weighted/adaptive process could be better. */
    if (0 == (tactic & 0x01)) {
        drop_from_bit_pool(t, env, orig, copy);
    } else {
        mutate_bit_pool(t, env, orig, copy);
    }
    LOG("========== AFTER\n");
    if (ENABLE_LOG) {
        theft_autoshrink_dump_bit_pool(stdout, copy->size,
            copy, THEFT_AUTOSHRINK_PRINT_ALL);
    }

    if (!env->leave_trailing_zeroes) {
        truncate_trailing_zero_bytes(copy);
    }

    void *res = NULL;
    enum theft_alloc_res ares = alloc_from_bit_pool(t, env, copy, &res);
    if (ares == THEFT_ALLOC_SKIP) {
        theft_autoshrink_free_bit_pool(t, copy);
        return THEFT_SHRINK_DEAD_END;
    } else if (ares == THEFT_ALLOC_ERROR) {
        theft_autoshrink_free_bit_pool(t, copy);
        return THEFT_SHRINK_ERROR;
    }

    assert(ares == THEFT_ALLOC_OK);
    copy->instance = res;
    *output = copy;
    return THEFT_SHRINK_OK;
}

static void
truncate_trailing_zero_bytes(struct theft_autoshrink_bit_pool *pool) {
    size_t nsize = 0;
    const size_t byte_size = (pool->size / 8)
      + ((pool->size % 8) == 0 ? 0 : 1);
    if (byte_size > 0) {
        for (size_t i = byte_size - 1; i > 0; i--) {
            if (pool->bits[i] != 0x00) {
                nsize = i + 1;
                break;
            }
        }
    }
    nsize *= 8;
    LOG("Truncating to nsize: %zd\n", nsize);
    pool->size = nsize;
}

static uint8_t popcount(uint64_t value) {
    uint8_t pop = 0;
    for (uint8_t i = 0; i < 64; i++) {
        if (value & (1LLU << i)) {
            pop++;
        }
    }
    return pop;
}

static uint8_t log2ceil(size_t value) {
    uint8_t res = 0;
    while ((1LLU << res) < value) {
        res++;
    }
    return res;
}

/* Copy the contents of the orig pool into the new pool, but with a
 * small probability of dropping individual requests. */
static void drop_from_bit_pool(struct theft *t,
    struct theft_autoshrink_env *env,
    const struct theft_autoshrink_bit_pool *orig,
    struct theft_autoshrink_bit_pool *copy) {
    size_t src_offset = 0;
    size_t dst_offset = 0;

    size_t src_byte = 0;
    size_t dst_byte = 0;
    uint8_t src_bit = 0x01;
    uint8_t dst_bit = 0x01;

    /* If N random bits are <= DROP_THRESHOLD, then drop the
     * current request, otherwise copy it.
     *
     * TODO: should this dynamically adjust based on orig->request_count? */
    const uint64_t drop_threshold = GET_DEF(env->drop_threshold, DEF_DROP_THRESHOLD);
    const uint8_t drop_bits = GET_DEF(env->drop_bits, DEF_DROP_BITS);

    autoshrink_prng_fun *prng = get_prng(t, env);

    /* Always drop at least one, unless to_drop is DO_NOT_DROP. */
    size_t to_drop = prng(32, env->udata);
    if (to_drop != DO_NOT_DROP) {
        to_drop %= orig->request_count;
    }

    size_t drop_count = 0;

    for (size_t ri = 0; ri < orig->request_count; ri++) {
        const uint32_t req_size = orig->requests[ri];
        if (ri == to_drop || prng(drop_bits, env->udata) <= drop_threshold) {
            LOG("DROPPING: %zd - %zd\n", src_offset, src_offset + req_size);
            drop_count++;
            for (size_t bi = 0; bi < req_size; bi++) {  // drop
                src_bit <<= 1;
                if (src_bit == 0x00) {
                    src_bit = 0x01;
                    src_byte++;
                }
                src_offset++;
            }
        } else {  // copy
            for (size_t bi = 0; bi < req_size; bi++) {
                if (orig->bits[src_byte] & src_bit) {
                    copy->bits[dst_byte] |= dst_bit;
                }

                src_bit <<= 1;
                if (src_bit == 0x00) {
                    src_bit = 0x01;
                    src_byte++;
                }
                src_offset++;

                dst_bit <<= 1;
                if (dst_bit == 0x00) {
                    dst_bit = 0x01;
                    dst_byte++;
                }
                dst_offset++;
            }
        }
    }

    for (size_t bi = src_offset; bi < orig->size; bi++) {
        if (orig->bits[src_byte] & src_bit) {
            copy->bits[dst_byte] |= dst_bit;
        }

        src_bit <<= 1;
        if (src_bit == 0x00) {
            src_bit = 0x01;
            src_byte++;
        }
        src_offset++;

        dst_bit <<= 1;
        if (dst_bit == 0x00) {
            dst_bit = 0x01;
            dst_byte++;
        }
        dst_offset++;
    }

    LOG("DROP: %zd -> %zd (%zd requests)\n",
        orig->size, dst_offset, drop_count);
    (void)drop_count;
    LOG("drop, new pool size SIZE %zd -> %zd\n",
        copy->size, dst_offset);
    copy->size = dst_offset;
}

#define MAX_CHANGES 5

static void mutate_bit_pool(struct theft *t,
                            struct theft_autoshrink_env *env,
                            const struct theft_autoshrink_bit_pool *orig,
                            struct theft_autoshrink_bit_pool *pool) {
    const size_t orig_bytes = (orig->size / 8) + ((orig->size % 8) == 0 ? 0 : 1);
    memcpy(pool->bits, orig->bits, orig_bytes);

    autoshrink_prng_fun *prng = get_prng(t, env);

    /* Ensure that we aren't getting random bits from a pool while trying
     * to shrink the pool. */
    assert(t->bit_pool == NULL);

    /* Get some random bits, and for each 1 bit, we will make one change in
     * the pool copy. */
    const uint8_t change_count = popcount(prng(MAX_CHANGES, env->udata)) + 1;
    uint8_t changed = 0;

    /* Attempt to make up to CHANGE_COUNT changes, with limited retries
     * for when the random modifications have no effect. */
    for (size_t i = 0; i < 10*change_count; i++) {
        if (choose_and_mutate_request(t, env, orig, pool)) {
            changed++;
            if (changed == change_count) {
                break;
            }
        }
    }

    /* Truncate half of the unconsumed bits  */
    size_t nsize = orig->consumed + (orig->size - orig->consumed)/2;
    pool->size = nsize < pool->size ? nsize : pool->size;

}

static bool
choose_and_mutate_request(struct theft *t,
                          struct theft_autoshrink_env *env,
                          const struct theft_autoshrink_bit_pool *orig,
                          struct theft_autoshrink_bit_pool *pool) {

    autoshrink_prng_fun *prng = get_prng(t, env);
    enum mutation mtype = (enum mutation)prng(MUTATION_TYPE_BITS, env->udata);

    const uint8_t request_bits = log2ceil(orig->request_count);

    /* Align a change in the bit pool with a random request. The
     * mod here biases it towards earlier requests. */
    const size_t pos = prng(request_bits, env->udata) % orig->request_count;
    const size_t bit_offset = offset_of_pos(orig, pos);
    const uint32_t size = orig->requests[pos];

    switch (mtype) {
    default:
        assert(false);
    case MUT_SHIFT:
    {
        uint8_t shift = prng(2, env->udata) + 1;
        if (size > 64) {
            assert(false); // TODO -- bulk requests
        } else {
            const uint64_t bits = read_bits_at_offset(pool, bit_offset, size);
            const uint64_t nbits = bits >> shift;
            LOG("SHIFT[%u, %u @ %zd (0x%08zx)]: 0x%016lx -> 0x%016lx\n",
                shift, size, pos, bit_offset, bits, nbits);
            write_bits_at_offset(pool, bit_offset, size, nbits);
            return (bits != nbits);
        }
        return false;
    }
    case MUT_MASK:
    {
        /* Clear each bit with 1/4 probability */
        uint64_t mask = prng(size, env->udata) | prng(size, env->udata);
        if (mask == (1LU << size) - 1) {
            // always clear at least 1 bit
            const uint8_t one_bit = prng(8, env->udata) % size;
            mask &=- (1LU << one_bit);
        }
        if (size > 64) {
            assert(false); // TODO -- bulk requests
        } else {
            const uint64_t bits = read_bits_at_offset(pool, bit_offset, size);
            const uint64_t nbits = bits & mask;
            LOG("MASK[0x%016lx, %u @ %zd (0x%08zx)]: 0x%016lx -> 0x%016lx\n",
                mask, size, pos, bit_offset, bits, nbits);
            write_bits_at_offset(pool, bit_offset, size, nbits);
            return (bits != nbits);
        }
        return false;
    }
    case MUT_SWAP:
    {
        if (size > 64) {
            assert(false); // TODO -- bulk requests
        } else {
            LOG("SWAP at %zd...\n", pos);
            const uint64_t bits = read_bits_at_offset(pool, bit_offset, size);

            /* Find the next pos of the same size, if any.
             * Read both, and if the latter is lexicographically smaller, swap. */
            for (size_t i = pos + 1; i < orig->request_count; i++) {
                if (orig->requests[i] == size) {
                    const size_t other_offset = offset_of_pos(orig, i);
                    const uint64_t other = read_bits_at_offset(pool, other_offset, size);
                    if (other < bits) {
                        LOG("SWAPPING %zd <-> %zd\n", pos, i);
                        write_bits_at_offset(pool, bit_offset, size, other);
                        write_bits_at_offset(pool, other_offset, size, bits);
                        return true;
                    }
                }
            }
        }
        return false;
    }
    case MUT_SUB:
    {
        const uint64_t sub = prng(size, env->udata);
        if (size > 64) {
            assert(false); // TODO -- bulk requests
        } else {
            uint64_t bits = read_bits_at_offset(pool, bit_offset, size);
            if (bits > 0) {
                uint64_t nbits = bits - (sub % bits);
                LOG("SUB[%lu, %u @ %zd (0x%08zx)]: 0x%016lx -> 0x%016lx\n",
                    sub, size, pos, bit_offset, bits, nbits);
                if (nbits == bits) {
                    nbits--;
                }
                write_bits_at_offset(pool, bit_offset, size, nbits);
                return true;
            }
        }
        return false;
    }
    }
}

static size_t offset_of_pos(const struct theft_autoshrink_bit_pool *orig,
                            size_t pos) {
    size_t res = 0;
    for (size_t i = 0; i < pos; i++) {
        res += orig->requests[i];
    }
    return res;
}

static void convert_bit_offset(size_t bit_offset,
                               size_t *byte_offset, uint8_t *bit) {
    *byte_offset = bit_offset / 8;
    *bit = bit_offset % 8;
}

static uint64_t
read_bits_at_offset(const struct theft_autoshrink_bit_pool *pool,
                    size_t bit_offset, uint8_t size) {
    size_t byte = 0;
    uint8_t bit = 0;
    convert_bit_offset(bit_offset, &byte, &bit);
    uint64_t acc = 0;
    uint8_t bit_i = 0x01 << bit;

    for (uint8_t i = 0; i < size; i++) {
        //fprintf(stdout, "byte %zd, size %zd\n", byte, pool->size);
        if (pool->bits[byte] & bit_i) {
            acc |= (1LLU << i);
        }
        bit_i <<= 1;
        if (bit_i == 0) {
            byte++;
            bit_i = 0x01;
        }
    }

    return acc;
}

static void
write_bits_at_offset(struct theft_autoshrink_bit_pool *pool,
                     size_t bit_offset, uint8_t size, uint64_t bits) {
    size_t byte = 0;
    uint8_t bit = 0;
    convert_bit_offset(bit_offset, &byte, &bit);
    uint8_t bit_i = 0x01 << bit;

    for (uint8_t i = 0; i < size; i++) {
        if (bits & (1LLU << i)) {
            pool->bits[byte] |= bit_i;
        } else {
            pool->bits[byte] &=~ bit_i;
        }
        bit_i <<= 1;
        if (bit_i == 0) {
            byte++;
            bit_i = 0x01;
        }
    }
}

void theft_autoshrink_dump_bit_pool(FILE *f, size_t bit_count,
                                    const struct theft_autoshrink_bit_pool *pool,
                                    enum theft_autoshrink_print_mode print_mode) {
    fprintf(f, "\n-- autoshrink_bit_pool[%zd bits, %zd consumed, %zd requests] --\n",
        pool->size, pool->consumed, pool->request_count);
    bool prev = false;
    if (print_mode & THEFT_AUTOSHRINK_PRINT_BIT_POOL) {
        prev = true;
        const uint8_t *bits = pool->bits;
        const size_t byte_count = bit_count / 8;
        for (size_t i = 0; i < byte_count; i++) {
            const uint8_t byte = bits[i];
            fprintf(f, "%02x ", byte);
            if ((i & 0x0f) == 0x0f) {
                fprintf(f, "\n");
            } else if ((i & 0x03) == 0x03) {
                fprintf(f, " ");
            }
        }
        const uint8_t rem = bit_count % 8;
        if (rem != 0) {
            const uint8_t byte = bits[byte_count] & ((1U << rem) - 1);
            fprintf(f, "%02x", byte);
            if ((byte_count & 0x0f) == 0x0e) {
                fprintf(f, "\n");
                prev = false;
            }
        }
    }
    if (print_mode & THEFT_AUTOSHRINK_PRINT_REQUESTS) {
        if (prev) {
            fprintf(f, "\n");
        }
        size_t offset = 0;
        for (size_t i = 0; i < pool->request_count; i++) {
            uint32_t req_size = pool->requests[i];
            if (offset + req_size > pool->size) {
                req_size = pool->size - offset;
            }
            uint64_t bits = read_bits_at_offset(pool, offset, req_size);
            fprintf(f, "0x%lx (%u), ", bits, req_size);
            if ((i & 0x07) == 0x07) {
                fprintf(f, "\n");
            }
            offset += req_size;
        }
        if ((pool->request_count % 0x07) != 0x07) {
            fprintf(f, "\n");
        }
    }
}

static void
autoshrink_print(FILE *f, const void *instance, void *venv) {
    CHECK_ENV_CAST(env, venv);
    CHECK_BIT_POOL_CAST(pool, instance);

    /* If the user has a print callback defined, use it on
     * the instance, otherwise print the bit pool. */
    if (env->user_type_info.print) {
        env->user_type_info.print(f, pool->instance,
            env->user_type_info.env);
    }

    assert(pool->size >= pool->consumed);
    theft_autoshrink_dump_bit_pool(f,
        pool->consumed, pool,
        GET_DEF(env->print_mode, THEFT_AUTOSHRINK_PRINT_REQUESTS));
}

static bool append_request(struct theft_autoshrink_bit_pool *pool,
    uint32_t bit_count) {
    assert(pool);
    if (pool->request_count == pool->request_ceil) {  // grow
        size_t nceil = pool->request_ceil * 2;
        uint32_t *nrequests = realloc(pool->requests, nceil * sizeof(*nrequests));
        if (nrequests == NULL) {
            return false;
        }
        pool->requests = nrequests;
        pool->request_ceil = nceil;
    }

    if (0) {
        LOG("appending request %zd for %u bits\n",
            pool->request_count, bit_count);
    }
    pool->requests[pool->request_count] = bit_count;
    pool->request_count++;
    return true;
}

static uint64_t def_autoshrink_prng(uint8_t bits, void *udata) {
    struct theft *t = (struct theft *)udata;
    return theft_random_bits(t, bits);
}

static autoshrink_prng_fun *get_prng(struct theft *t,
                                     struct theft_autoshrink_env *env) {
    if (env->prng) {
        return env->prng;
    } else {
        env->udata = t;
        return def_autoshrink_prng;
    }
}
