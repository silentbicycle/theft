#ifndef THEFT_AUTOSHRINK_MODEL_H
#define THEFT_AUTOSHRINK_MODEL_H

#include "theft_types_internal.h"

enum autoshrink_action {
    ASA_DROP = 0x01,
    ASA_SHIFT = 0x02,
    ASA_MASK = 0x04,
    ASA_SWAP = 0x08,
    ASA_SUB = 0x10,
};

enum mutation {
    MUT_SHIFT,
    MUT_MASK,
    MUT_SWAP,
    MUT_SUB,
};
#define LAST_MUTATION MUT_SUB

struct autoshrink_model *
theft_autoshrink_model_new(struct theft *t);

void
theft_autoshrink_model_free(struct theft *t, struct autoshrink_model *m);

bool
theft_autoshrink_model_should_drop(prng_fun *prng, void *udata,
    struct autoshrink_model *model);

enum mutation
theft_autoshrink_model_get_weighted_mutation(struct theft *t,
    struct autoshrink_model *model);

void
theft_autoshrink_model_update(struct autoshrink_model *model,
    uint8_t arg_id);

/* Set the next action the model will deliver. (This is a hook for testing.) */
void theft_autoshrink_model_set_next(struct autoshrink_model *model,
    enum autoshrink_action action);

void
theft_autoshrink_model_notify_new_trial(struct autoshrink_model *m);

void
theft_autoshrink_model_notify_attempted(struct autoshrink_model *m,
    enum autoshrink_action action, bool useful);

void
theft_autoshrink_model_notify_trial_result(struct autoshrink_model *m,
    enum theft_trial_res res);

#endif
