#ifndef TEST_THEFT_H
#define TEST_THEFT_H

#include "greatest.h"
#include "theft.h"

#include <assert.h>
#include <inttypes.h>

/* Get a theft handle for testing functions that need one, but
 * don't ever actually run the test. Free with theft_run_free. */
struct theft *test_theft_init(void);

SUITE_EXTERN(prng);
SUITE_EXTERN(autoshrink);
SUITE_EXTERN(aux);
SUITE_EXTERN(bloom);
SUITE_EXTERN(error);
SUITE_EXTERN(integration);

#endif
