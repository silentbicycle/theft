#ifndef THEFT_AUTOSHRINK_MODEL_INTERNAL_H
#define THEFT_AUTOSHRINK_MODEL_INTERNAL_H

#include "theft_autoshrink_model.h"

enum autoshrink_weight {
    WEIGHT_DROP = 0x00,
    WEIGHT_SHIFT = 0x01,
    WEIGHT_MASK = 0x02,
    WEIGHT_SWAP = 0x03,
    WEIGHT_SUB = 0x04,
    WEIGHT_COUNT,
};

struct autoshrink_model {
    enum autoshrink_action cur_used;
    /* enum autoshrink_action cur_tried; */
    /* enum autoshrink_action cur_set; */
    enum autoshrink_action next_action;
    uint8_t weights[WEIGHT_COUNT];
};

#define TWO_EVENLY 0x80
#define FOUR_EVENLY 0x40
#define MODEL_MIN 0x08
#define MODEL_MAX 0x80

static void dump_weights(const char *label,
    struct autoshrink_model *model);

static enum autoshrink_weight
weight_of_action(enum autoshrink_action action);

static uint16_t get_weight_total(struct autoshrink_model *model);
static uint8_t get_weight_total_bit_ceil(struct autoshrink_model *model);

#endif
