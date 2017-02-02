#ifndef THEFT_SHRINK_INTERNAL_H
#define THEFT_SHRINK_INTERNAL_H

#include "theft_types_internal.h"

static enum shrink_res
attempt_to_shrink_arg(struct theft *t, struct theft_propfun_info *info,
    void *args[], void *env, int ai);

#endif
