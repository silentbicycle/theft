#ifndef TEST_THEFT_H
#define TEST_THEFT_H

#include "greatest.h"
#include "theft.h"

#include <assert.h>
#include <inttypes.h>

/* Allocate a theft handle with placeholder/no-op arguments. */
struct theft *test_theft_init(void);

SUITE_EXTERN(prng);
SUITE_EXTERN(autoshrink);
SUITE_EXTERN(aux);
SUITE_EXTERN(bloom);
SUITE_EXTERN(error);
SUITE_EXTERN(integration);

#endif
