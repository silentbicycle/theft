#include "theft_autoshrink_internal.h"

#include "theft_random.h"

#include <string.h>
#include <assert.h>

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
    };
    memcpy(&env->user_type_info, type_info, sizeof(*type_info));

    *wrapper = (struct theft_type_info) {
        .alloc = autoshrink_alloc,
        .free = autoshrink_free,
        .hash = autoshrink_hash,
        .shrink = autoshrink_shrink,
        .print = autoshrink_print,
        .autoshrink = true,
        .env = env,
    };

    return true;
}

uint64_t
theft_autoshrink_bit_pool_random(struct theft_autoshrink_bit_pool *pool,
    uint8_t bit_count) {
    assert(pool);
    assert(bit_count <= 64);

    if (pool->consumed == pool->size) {
        return 0;   // saturate at 0
    } else if (pool->consumed + bit_count >= pool->size) {
        bit_count = pool->size - pool->consumed;
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

static struct theft_autoshrink_bit_pool *alloc_bit_pool(size_t size) {
    struct theft_autoshrink_bit_pool *res = NULL;

    size_t alloc_size = get_aligned_size(size, 32);
    assert((alloc_size % 32) == 0);

    /* Ensure that the allocation size is aligned to 32 bits, so we can
     * work in 32-bit steps in drop_bits and mask_bits below. */
    uint32_t *aligned_bits = calloc(alloc_size/32, sizeof(uint32_t));
    uint8_t *bits = (uint8_t *)aligned_bits;
    if (bits == NULL) {
        return NULL;
    }

    res = calloc(1, sizeof(*res));
    if (res == NULL) {
        free(bits);
        return NULL;
    }

    *res = (struct theft_autoshrink_bit_pool) {
        .tag = AUTOSHRINK_BIT_POOL_TAG,
        .bits = bits,
        .size = size,
    };
    return res;
}

struct theft_autoshrink_bit_pool *
theft_autoshrink_init_bit_pool(struct theft *t,
        size_t size, theft_seed seed) {
    struct theft_autoshrink_bit_pool *res = NULL;
    res = alloc_bit_pool(size);
    if (res == NULL) {
        return NULL;
    }

    theft_set_seed(t, seed);
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
    free(pool);
}

void
theft_autoshrink_get_real_args(struct theft_run_info *run_info,
        void **dst, void **src) {
    for (size_t i = 0; i < run_info->arity; i++) {
        const struct theft_type_info *ti = run_info->type_info[i];
        if (ti->autoshrink) {
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

    /* FIXME: make this starting size configurable */
    size_t pool_size = DEF_POOL_SIZE;
    struct theft_autoshrink_bit_pool *pool =
      theft_autoshrink_init_bit_pool(t, pool_size, seed);
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
        /* FIXME: only hash consumed bits of last byte */
        size_t pool_bytes = pool->consumed / 8 + ((pool->consumed % 8) == 0 ? 0 : 1);
        return theft_hash_onepass(pool->bits, pool_bytes);
    }
}

static enum theft_shrink_res
autoshrink_shrink(struct theft *t, const void *instance, uint32_t tactic,
    void *venv, void **output) {
    CHECK_ENV_CAST(env, venv);

    uint32_t ti = 0;            /* tactic counter */

    const struct theft_autoshrink_bit_pool *orig
      = (struct theft_autoshrink_bit_pool *)instance;
    assert(orig->tag == AUTOSHRINK_BIT_POOL_TAG);

    struct theft_autoshrink_bit_pool *copy = alloc_bit_pool(orig->size);
    if (copy == NULL) {
        return THEFT_SHRINK_ERROR;
    }
    bool shrunk = false;

    while (ti < MAX_AUTOSHRINKS) {
        /* Other heuristics:  */

        if (ti++ == tactic) {
            uint8_t bits = theft_random_bits(t, 3);
            if (bits == 0) {
                bits = 1;
            } else {
                bits = 8 * bits; // byte-alignment
            }
            /* printf("TACTIC %u / %u: drop %u\n", tactic, MAX_AUTOSHRINKS, bits); */
            drop_bits(t, bits, copy, orig);
            shrunk = true;
        } else if (ti++ == tactic) {
            uint8_t bits = theft_random_bits(t, 3);
            if (bits < 2) {
                bits = 1;
            } else {
                bits = 8 * bits/2; // byte-alignment
            }
            /* printf("TACTIC %u / %u: mask %u\n", tactic, MAX_AUTOSHRINKS, bits); */
            mask_bits(t, bits, copy, orig);
            shrunk = true;
        }

        if (shrunk) {
            break;
        }
    }

    if (shrunk) {
        /* truncate size to last non-zero byte */
        size_t nsize = 0;
        const size_t byte_size = (copy->size / 8)
          + ((copy->size % 8) == 0 ? 0 : 1);
        if (byte_size > 0) {
            for (size_t i = byte_size - 1; i > 0; i--) {
                if (copy->bits[i] != 0x00) {
                    nsize = i + 1;
                    break;
                }
            }
        }
        nsize *= 8;
        copy->size = nsize;

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
    } else {
        theft_autoshrink_free_bit_pool(t, copy);
        return THEFT_SHRINK_NO_MORE_TACTICS;
    }
}

static void drop_bits(struct theft *t, uint8_t bits,
        struct theft_autoshrink_bit_pool *dst,
        const struct theft_autoshrink_bit_pool *src) {
    const size_t aligned_size = get_aligned_size(src->size, 32);
    assert(dst != src);
    uint8_t *bits_out = dst->bits;
    const uint8_t *bits_in = src->bits;

    size_t out_offset = 0;
    uint32_t out_bit = 0x01;

    int16_t drop_or_keep = 0;

    for (size_t bi = 0; bi < aligned_size; bi++) {
        if (drop_or_keep == 0) {
            /* If draw is 0 (out of 16), then drop the next BITS bits,
             * otherwise keep the next BITS * draw bits. Drop in
             * larger quantities when after the bits that were
             * consumed last round. */
            const uint8_t draw = theft_random_bits(t, 4);
            if (draw == 0) {
                drop_or_keep = -bits;
                if (bi >= 2*src->consumed) {
                    drop_or_keep = -1024;
                }
            } else {
                drop_or_keep = draw * bits;
            }
        }

        if (drop_or_keep < 0) {
            drop_or_keep++;
        } else if (drop_or_keep > 0) {
            drop_or_keep--;
            bits_out[out_offset] |= bits_in[bi / 8] & (1LU << (bi & 0x07));
            if (out_bit == 0x80) {
                out_bit = 0x01;
                out_offset++;
            } else {
                out_bit <<= 1;
            }
        }
    }

    dst->size = 8*(out_offset + (out_bit == 0x01 ? 0 : 1));
    /* printf("DROP: in %zd, out %zd (out_offset %zd)\n", */
    /*     src->size, dst->size, out_offset); */
}

static void mask_bits(struct theft *t, uint8_t bits,
        struct theft_autoshrink_bit_pool *dst,
        const struct theft_autoshrink_bit_pool *src) {
    const size_t aligned_size = get_aligned_size(src->size, 32);

    const uint8_t *in = src->bits;
    uint8_t *out = dst->bits;

    int16_t mask_count = 0;
    for (size_t bi = 0; bi < aligned_size; bi++) {
        if (mask_count == 0) {
            const uint8_t draw = theft_random_bits(t, 4);
            if (draw == 0) {
                mask_count = -bits;
                if (bi >= 2*src->consumed) {
                    mask_count = -1024;
                }
            } else {
                mask_count = draw * bits/8;
                if (bi >= src->consumed) {
                    bits /= 4;
                }
            }
        }

        if (mask_count < 0) {
            mask_count++;
        } else if (mask_count > 0) {
            out[bi / 8] |= (in[bi / 8] & (1LU << (bi & 0x07)));
            mask_count--;
        }
    }

#if 0
    printf("in:\n");
    for (size_t i = 0; i < aligned_size/8; i++) {
        printf("%02x ", in[i]);
        if (0 == (i & 15)) {
            printf("\n");
        }
    }
    printf("\nout:\n");
    for (size_t i = 0; i < aligned_size/8; i++) {
        printf("%02x%c", out[i], out[i] == in[i] ? ' ' : '<');
        if (0 == (i & 15)) {
            printf("\n");
        }
    }
    printf("\n");
#endif

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
    } /*else*/ {
        // TODO: make printing this configurable
        fprintf(f, "\n-- autoshrink_bit_pool[%zd bits, %zd consumed] --\n",
            pool->size, pool->consumed);
        const uint8_t *bits = pool->bits;
        assert(pool->size >= pool->consumed);
        const size_t byte_count = pool->consumed / 8 + (pool->consumed % 8 == 0 ? 0 : 1);
        for (size_t i = 0; i < byte_count; i++) {
            const uint8_t byte = bits[i];
            fprintf(f, "%02x ", byte);
            if ((i & 0x0f) == 0x0f) {
                fprintf(f, "\n");
            } else if ((i & 0x03) == 0x03) {
                fprintf(f, " ");
            }
        }
    }
}
