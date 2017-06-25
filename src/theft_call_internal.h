#ifndef THEFT_CALL_INTERNAL_H
#define THEFT_CALL_INTERNAL_H

#include "theft_call.h"
#include "theft_bloom.h"
#include <assert.h>

#include <unistd.h>
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include <errno.h>

static enum theft_trial_res
theft_call_inner(struct theft_run_info *run_info, void **args);

static enum theft_trial_res
parent_handle_child_call(struct theft_run_info *run_info,
    pid_t pid, int fd);

#endif
