#include "theft_autoshrink_model_internal.h"

#define LOG_AUTOSHRINK_MODEL 0

struct autoshrink_model *
theft_autoshrink_model_new(struct theft *t) {
    (void)t;
    struct autoshrink_model *res = calloc(1, sizeof(*res));
    if (res == NULL) { return NULL; }

    *res = (struct autoshrink_model) {
        .weights = {
            [WEIGHT_DROP] = FOUR_EVENLY,
            [WEIGHT_SHIFT] = FOUR_EVENLY,
            [WEIGHT_MASK] = FOUR_EVENLY,
            [WEIGHT_SWAP] = FOUR_EVENLY - 0x10,
            [WEIGHT_SUB] = FOUR_EVENLY,
        },
    };

    return res;
}

void
theft_autoshrink_model_free(struct theft *t, struct autoshrink_model *m) {
    (void)t;
    dump_weights("FREE", m);
    free(m);
}

void
theft_autoshrink_model_notify_new_trial(struct autoshrink_model *m) {
    (void)m;
    m->cur_used = 0x00;
}

void
theft_autoshrink_model_notify_attempted(struct autoshrink_model *m,
        enum autoshrink_action action, bool useful) {
    enum autoshrink_weight w = weight_of_action(action);
    if (useful) {
        m->cur_used |= action;
        if (m->weights[w] < MODEL_MAX) {
            m->weights[w] += 0x02;
            LOG(4 - LOG_AUTOSHRINK_MODEL,
                "%s: increased weight %d to %u\n",
                __func__, w, m->weights[w]);
        }
    } else {
        if (m->weights[w] > MODEL_MIN) {
            m->weights[w] -= 0x02;
            LOG(4 - LOG_AUTOSHRINK_MODEL,
                "%s: decreased weight %d to %u\n",
                __func__, w, m->weights[w]);
        }
    }
}

void
theft_autoshrink_model_notify_trial_result(struct autoshrink_model *m,
        enum theft_trial_res res) {
    LOG(3 - LOG_AUTOSHRINK_MODEL,
        "%s: used action mask 0x%02x\n", __func__, (uint8_t)m->cur_used);
    dump_weights("BEFORE_TRIAL_RESULT", m);

    for (enum autoshrink_action a = ASA_DROP; a <= ASA_SUB; a <<= 1) {
        if (m->cur_used & a) {
            enum autoshrink_weight w = weight_of_action(a);
            switch (res) {
            case THEFT_TRIAL_FAIL:
                m->weights[w] += 0x08;
                if (m->weights[w] > MODEL_MAX) {
                    m->weights[w] = MODEL_MAX;
                }
                LOG(4 - LOG_AUTOSHRINK_MODEL,
                    "%s: increased weight %d to %u\n",
                    __func__, w, m->weights[w]);
                break;
            case THEFT_TRIAL_PASS:
                m->weights[w] -= 0x08;
                if (m->weights[w] < MODEL_MIN) {
                    m->weights[w] = MODEL_MIN;
                }
                LOG(4 - LOG_AUTOSHRINK_MODEL,
                    "%s: decreased weight %d to %u\n",
                    __func__, w, m->weights[w]);
                break;
            case THEFT_TRIAL_SKIP:
                m->weights[w] -= 0x03; /* slight decrease */
                if (m->weights[w] < MODEL_MIN) {
                    m->weights[w] = MODEL_MIN;
                }
                LOG(4 - LOG_AUTOSHRINK_MODEL,
                    "%s: decreased weight %d to %u\n",
                    __func__, w, m->weights[w]);
                break;
            default:
                LOG(3 - LOG_AUTOSHRINK_MODEL,
                    "%s: ignoring res %d\n", __func__, res);
            }
        }
    }

    uint16_t total = 0;
    bool all_div_2 = true;
    for (enum autoshrink_weight w = 0; w < WEIGHT_COUNT; w++) {
        if (m->weights[w] & 0x01) { all_div_2 = false; }
        total += m->weights[w];
    }

    if (total < 0x80) {
        for (enum autoshrink_weight w = 0; w < WEIGHT_COUNT; w++) {
            m->weights[w] *= 2;
        }
    } else if (total > 0x100 && all_div_2) {
        for (enum autoshrink_weight w = 0; w < WEIGHT_COUNT; w++) {
            m->weights[w] /= 2;
        }
    }

    dump_weights("AFTER_TRIAL_RESULT", m);
}

void theft_autoshrink_model_set_next(struct autoshrink_model *m,
    enum autoshrink_action action) {
    m->next_action = action;
}

bool
theft_autoshrink_model_should_drop(prng_fun *prng, void *prng_udata,
        struct autoshrink_model *model) {
    uint8_t weight = model->weights[WEIGHT_DROP];
    uint16_t total = get_weight_total(model);
    uint8_t bits = get_weight_total_bit_ceil(model);
    assert((1LLU << bits) >= total);
    assert(1LLU << (bits - 1) < total);
    
    if (model->next_action == 0x00) {
        uint16_t draw;
        do {
            draw = prng(bits, prng_udata);
        } while(draw >= total);
        LOG(3 - LOG_AUTOSHRINK_MODEL, "%s: draw %u, weight %u\n",
            __func__, draw, weight);
        return draw < weight;
    } else {
        return model->next_action == ASA_DROP;
    }
}

static enum autoshrink_weight
weight_of_action(enum autoshrink_action action) {
    switch (action) {
    default: assert(false);
    case ASA_DROP: return WEIGHT_DROP;
    case ASA_SHIFT: return WEIGHT_SHIFT;
    case ASA_MASK: return WEIGHT_MASK;
    case ASA_SWAP: return WEIGHT_SWAP;
    case ASA_SUB: return WEIGHT_SUB;
    }
}

static void dump_weights(const char *label, struct autoshrink_model *model) {
    LOG(3 - LOG_AUTOSHRINK_MODEL,
        "%s: drop %04x, shift %04x, mask %04x, swap %04x, sub %04x\n",
        label,
        model->weights[WEIGHT_DROP],
        model->weights[WEIGHT_SHIFT],
        model->weights[WEIGHT_MASK],
        model->weights[WEIGHT_SWAP],
        model->weights[WEIGHT_SUB]);
}

enum mutation
theft_autoshrink_model_get_weighted_mutation(struct theft *t,
    struct autoshrink_model *model) {
    if (model->next_action != 0x00) {
        switch (model->next_action) {
        default: assert(false);
        case ASA_SHIFT:
            return MUT_SHIFT;
        case ASA_MASK:
            return MUT_MASK;
        case ASA_SWAP:
            return MUT_SWAP;
        case ASA_SUB:
            return MUT_SUB;
        }
    }

    /* Note: drop is handled separately */
    const uint16_t shift = model->weights[WEIGHT_SHIFT];
    const uint16_t mask = shift + model->weights[WEIGHT_MASK];
    const uint16_t swap = mask + model->weights[WEIGHT_SWAP];
    const uint16_t sub = swap + model->weights[WEIGHT_SUB];

    if (THEFT_LOG_LEVEL >= 4 - LOG_AUTOSHRINK_MODEL) {
        dump_weights(__func__, model);
    }

    uint8_t bit_count = 5;
    while ((1LU << bit_count) < sub) {
        bit_count++;
    }
    assert(bit_count <= 16);

    for (;;) {
        #define OFFSET 0
        const uint16_t bits = theft_random_bits(t, bit_count);
        LOG(4 - LOG_AUTOSHRINK_MODEL - OFFSET,
            "%s: 0x%04x / 0x%04x -- ", __func__, bits, sub);
        if (bits < shift) {
            LOG(4 - LOG_AUTOSHRINK_MODEL - OFFSET, "SHIFT\n");
            return MUT_SHIFT;
        } else if (bits < mask) {
            LOG(4 - LOG_AUTOSHRINK_MODEL - OFFSET, "MASK\n");
            return MUT_MASK;
        } else if (bits < swap) {
            LOG(4 - LOG_AUTOSHRINK_MODEL - OFFSET, "SWAP\n");
            return MUT_SWAP;
        } else if (bits < sub) {
            LOG(4 - LOG_AUTOSHRINK_MODEL - OFFSET, "SUB\n");
            return MUT_SUB;
        } else {
            LOG(4 - LOG_AUTOSHRINK_MODEL - OFFSET, "continue\n");
            continue;  // draw again
        }
    }
}

static uint16_t get_weight_total(struct autoshrink_model *model) {
    uint16_t total = 0;
    for (enum autoshrink_weight w = 0; w < WEIGHT_COUNT; w++) {
        total += model->weights[w];
    }
    return total;
}

static uint8_t get_weight_total_bit_ceil(struct autoshrink_model *model) {
    const uint16_t total = get_weight_total(model);
    uint8_t bits = 5;
    while ((1LLU << bits) < total) {
        bits++;
        assert(bits <= 16);
    }
    return bits;
}
