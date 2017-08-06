#ifndef THEFT_RUN_H
#define THEFT_RUN_H

enum theft_run_init_res {
    THEFT_RUN_INIT_OK,
    THEFT_RUN_INIT_ERROR_MEMORY = -1,
    THEFT_RUN_INIT_ERROR_BAD_ARGS = -2,
};
enum theft_run_init_res
theft_run_init(const struct theft_run_config *cfg,
    struct theft **output);

/* Actually run the trials, with all arguments made explicit. */
enum theft_run_res
theft_run_trials(struct theft *t);

void
theft_run_free(struct theft *t);

#endif
