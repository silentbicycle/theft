#ifndef THEFT_CALL_H
#define THEFT_CALL_H

#include "theft_types_internal.h"

/* Actually call the property function referenced in INFO,
 * with the arguments in ARGS. */
enum theft_trial_res
theft_call(struct theft *t, void **args);

/* Check if this combination of argument instances has been called. */
bool theft_call_check_called(struct theft *t);

/* Mark the tuple of argument instances as called in the bloom filter. */
void theft_call_mark_called(struct theft *t);


#endif
