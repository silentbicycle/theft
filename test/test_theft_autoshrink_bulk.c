#include "test_theft.h"
#include "test_theft_autoshrink_bulk.h"

static void bb_free(void *instance, void *env);
static void bb_print(FILE *f, const void *instance, void *env);

/* Generate a block of random bits, with a known size.
 * This is mainly to exercise `theft_random_bits_bulk`. */
static enum theft_alloc_res
bb_alloc(struct theft *t, void *env, void **instance) {
    (void)env;
    size_t size = theft_random_bits(t, 20);
    uint64_t *buf = calloc(size / 64 + 1, sizeof(uint64_t));
    if (buf == NULL) {
        return THEFT_ALLOC_ERROR;
    }

    struct bulk_buffer *bb = calloc(1, sizeof(*bb));
    if (bb == NULL) {
        free(buf);
        return THEFT_ALLOC_ERROR;
    }
    *bb = (struct bulk_buffer) {
        .size = size,
        .buf = buf,
    };

    theft_random_bits_bulk(t, size, bb->buf);
    *instance = bb;
    //bb_print(stdout, bb, NULL);

    return THEFT_ALLOC_OK;
}

static void
bb_free(void *instance, void *env) {
    (void)env;
    struct bulk_buffer *bb = (struct bulk_buffer *)instance;
    free(bb->buf);
    free(bb);
}

static void bb_print(FILE *f, const void *instance, void *env) {
    (void)env;
    struct bulk_buffer *bb = (struct bulk_buffer *)instance;
    fprintf(f, "-- bulk_buf[%zd]: \n", bb->size);
    const uint8_t *buf8 = (uint8_t *)bb->buf;
    const size_t limit = bb->size/8;
    for (size_t offset = 0; offset < limit; offset += 16) {
        const size_t rem = (limit - offset < 16 ? limit - offset : 16);
        for (size_t i = 0; i < rem; i++) {
            fprintf(f, "%02x ", buf8[offset + i]);
            if (i == 15) {
                fprintf(f, "\n");
            }
        }
    }

    const size_t rem_bits = (bb->size % 8);
    if (rem_bits != 0) {
        fprintf(f, "%02x/%zd\n", buf8[limit], rem_bits);
    } else {
        fprintf(f, "\n");
    }
}

struct theft_type_info bb_info = {
    .alloc = bb_alloc,
    .free = bb_free,
    .print = bb_print,
    .autoshrink_config = {
        .enable = true,
        .print_mode = THEFT_AUTOSHRINK_PRINT_ALL,
    },
};
