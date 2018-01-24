#ifndef THEFT_SHRINK_H
#define THEFT_SHRINK_H

enum shrink_res {
    SHRINK_OK,                  /* simplified argument further */
    SHRINK_DEAD_END,            /* at local minima */
    SHRINK_ERROR,               /* hard error during shrinking */
    SHRINK_HALT,                /* don't shrink any further */
};

/* Attempt to simplify all arguments, breadth first. Continue as long as
 * progress is made, i.e., until a local minimum is reached. */
bool
theft_shrink(struct theft *t);

#endif
