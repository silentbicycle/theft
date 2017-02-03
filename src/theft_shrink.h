#ifndef THEFT_SHRINK_H
#define THEFT_SHRINK_H

/* Attempt to simplify all arguments, breadth first. Continue as long as
 * progress is made, i.e., until a local minima is reached. */
bool
theft_shrink(struct theft *t,
    struct theft_run_info *info,
    struct theft_trial_info *trial_info);

#endif
