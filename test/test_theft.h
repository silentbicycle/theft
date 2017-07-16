#ifndef TEST_THEFT_H
#define TEST_THEFT_H

#include "greatest.h"
#include "theft.h"

#include <assert.h>

/* These are not part of the public API, but are exposed for testing. */
struct theft *theft_init(uint8_t bloom_bits);
void theft_free(struct theft *t);

SUITE_EXTERN(prng);
SUITE_EXTERN(autoshrink);
SUITE_EXTERN(aux);
SUITE_EXTERN(bloom);
SUITE_EXTERN(error);
SUITE_EXTERN(integration);

#endif
