#include "theft_call_internal.h"
#include "theft_autoshrink.h"

#include <time.h>
#include <sys/time.h>

#define LOG_CALL 0

#define MAX_FORK_RETRIES 10
#define DEF_KILL_SIGNAL SIGTERM

/* Actually call the property function. Its number of arguments is not
 * constrained by the typedef, but will be defined at the call site
 * here. (If info->arity is wrong, it will probably crash.) */
enum theft_trial_res
theft_call(struct theft *t, void **args) {
    enum theft_trial_res res = THEFT_TRIAL_ERROR;

    if (t->fork.enable) {
        struct timespec tv = { .tv_nsec = 1 };
        if (-1 == pipe(t->workers[0].fds)) { return THEFT_TRIAL_ERROR; }

        pid_t pid = -1;
        for (;;) {
            pid = fork();
            if (pid == -1) {
                if (errno == EAGAIN) {
                    /* If we get EAGAIN, then wait for terminated
                     * child processes a chance to clean up -- forking
                     * is probably failing due to RLIMIT_NPROC. */
                    const int fork_errno = errno;
                    if (!step_waitpid(t)) { return THEFT_TRIAL_ERROR; }
                    if (-1 == nanosleep(&tv, NULL)) {
                        perror("nanosleep");
                        return THEFT_TRIAL_ERROR;
                    }
                    if (tv.tv_nsec >= (1L << MAX_FORK_RETRIES)) {
                        errno = fork_errno;
                        perror("fork");
                        return THEFT_TRIAL_ERROR;
                    }
                    errno = 0;
                    tv.tv_nsec <<= 1;
                    continue;
                } else {
                    perror("fork");
                    return THEFT_TRIAL_ERROR;
                }
            } else {
                break;
            }
        }

        if (pid == -1) {
            close(t->workers[0].fds[0]);
            close(t->workers[0].fds[1]);
            return THEFT_TRIAL_ERROR;
        } else if (pid == 0) {  /* child */
            close(t->workers[0].fds[0]);
            int out_fd = t->workers[0].fds[1];
            if (run_fork_post_hook(t, args) == THEFT_HOOK_FORK_POST_ERROR) {
                uint8_t byte = (uint8_t)THEFT_TRIAL_ERROR;
                ssize_t wr = write(out_fd, (const void *)&byte, sizeof(byte));
                (void)wr;
                exit(EXIT_FAILURE);
            }
            res = theft_call_inner(t, args);
            uint8_t byte = (uint8_t)res;
            ssize_t wr = write(out_fd, (const void *)&byte, sizeof(byte));
            exit(wr == 1 && res == THEFT_TRIAL_PASS
                ? EXIT_SUCCESS
                : EXIT_FAILURE);
        } else {                /* parent */
            close(t->workers[0].fds[1]);
            t->workers[0].pid = pid;

            t->workers[0].state = WS_ACTIVE;
            res = parent_handle_child_call(t, pid, &t->workers[0]);
            close(t->workers[0].fds[0]);
            t->workers[0].state = WS_INACTIVE;

            if (!step_waitpid(t)) { return THEFT_TRIAL_ERROR; }
            return res;
        }
    } else {                    /* just call */
        res = theft_call_inner(t, args);
    }
    return res;
}

static enum theft_trial_res
parent_handle_child_call(struct theft *t, pid_t pid, struct worker_info *worker) {
    const int fd = worker->fds[0];
    struct pollfd pfd[1] = {
        { .fd = fd, .events = POLLIN },
    };
    const int timeout = t->fork.timeout;
    int res = 0;
    for (;;) {
        struct timeval tv_pre = { 0, 0 };
        gettimeofday(&tv_pre, NULL);
        res = poll(pfd, 1, (timeout == 0 ? -1 : timeout));
        struct timeval tv_post = { 0, 0 };
        gettimeofday(&tv_post, NULL);

        const size_t delta = 1000*tv_post.tv_sec - 1000*tv_pre.tv_sec +
          ((tv_post.tv_usec / 1000) - (tv_pre.tv_usec / 1000));
        LOG(3 - LOG_CALL,"%s: POLL res %d, elapsed %zd\n",
            __func__, res, delta);
        (void)delta;

        if (res == -1) {
            if (errno == EAGAIN) {
                errno = 0;
                continue;
            } else if (errno == EINTR) {
                errno = 0;
                continue;
            } else {
                return THEFT_TRIAL_ERROR;
            }
        } else {
            break;
        }
    }

    if (res == 0) {     /* timeout */
        int kill_signal = t->fork.signal;
        if (kill_signal == 0) {
            kill_signal = DEF_KILL_SIGNAL;
        }
        LOG(2 - LOG_CALL, "%s: kill(%d, %d)\n",
            __func__, pid, kill_signal);
        assert(pid != -1);      /* do not do this. */
        if (-1 == kill(pid, kill_signal)) {
            return THEFT_TRIAL_ERROR;
        }

        /* Check if kill's signal made the child process terminate (or
         * if it exited successfully, and there was just a race on the
         * timeout). If so, save its exit status.
         *
         * If it still hasn't exited after the exit_timeout, then
         * send it SIGKILL and wait for _that_ to make it exit. */
        const size_t kill_time = 10; /* time to exit after SIGKILL */
        const size_t timeout_msec = (t->fork.exit_timeout == 0
            ? THEFT_DEF_EXIT_TIMEOUT_MSEC
            : t->fork.exit_timeout);

        /* After sending the signal to the timed out process,
         * give it timeout_msec to actually exit (in case a custom
         * signal is triggering some sort of cleanup) before sending
         * SIGKILL and waiting up to kill_time it to change state. */
        if (!wait_for_exit(t, worker, timeout_msec, kill_time)) {
            return THEFT_TRIAL_ERROR;
        }

        /* If the child still exited successfully, then consider it a
         * PASS, even though it exceeded the timeout. */
        if (worker->state == WS_STOPPED) {
            const int st = worker->wstatus;
            LOG(2 - LOG_CALL, "exited? %d, exit_status %d\n",
                WIFEXITED(st), WEXITSTATUS(st));
            if (WIFEXITED(st) && WEXITSTATUS(st) == EXIT_SUCCESS) {
                return THEFT_TRIAL_PASS;
            }
        }

        return THEFT_TRIAL_FAIL;
    } else {
        /* As long as the result isn't a timeout, the worker can
         * just be cleaned up by the next batch of waitpid()s. */
        enum theft_trial_res trial_res = THEFT_TRIAL_ERROR;
        uint8_t res_byte = 0xFF;
        ssize_t rd = 0;
        for (;;) {
            rd = read(fd, &res_byte, sizeof(res_byte));
            if (rd == -1) {
                if (errno == EINTR) {
                    errno = 0;
                    continue;
                }
                return THEFT_TRIAL_ERROR;
            } else {
                break;
            }
        }

        if (rd == 0) {
            /* closed without response -> crashed */
            trial_res = THEFT_TRIAL_FAIL;
        } else {
            assert(rd == 1);
            trial_res = (enum theft_trial_res)res_byte;
        }

        return trial_res;
    }
}

/* Clean up after all child processes that have changed state.
 * Save the exit/termination status for worker processes. */
static bool
step_waitpid(struct theft *t) {
    int wstatus = 0;
    int old_errno = errno;
    for (;;) {
        errno = 0;
        pid_t res = waitpid(-1, &wstatus, WNOHANG);
        LOG(2 - LOG_CALL, "%s: waitpid? %d\n", __func__, res);
        if (res == -1) {
            if (errno == ECHILD) { break; } /* No Children */
            perror("waitpid");
            return THEFT_TRIAL_ERROR;
        } else if (res == 0) {
            break;   /* no children have changed state */
        } else {
            if (res == t->workers[0].pid) {
                t->workers[0].state = WS_STOPPED;
                t->workers[0].wstatus = wstatus;
            }
        }
    }
    errno = old_errno;
    return true;
}

/* Wait timeout msec. for the worker to exit. If kill_timeout is
 * non-zero, then send SIGKILL and wait that much longer. */
static bool
wait_for_exit(struct theft *t, struct worker_info *worker,
    size_t timeout, size_t kill_timeout) {
    for (size_t i = 0; i < timeout + kill_timeout; i++) {
        if (!step_waitpid(t)) { return false; }
        if (worker->state == WS_STOPPED) { break; }

        /* If worker hasn't exited yet and kill_timeout is
         * non-zero, send SIGKILL. */
        if (i == timeout) {
            assert(kill_timeout > 0);
            assert(worker->pid != -1);
            int kill_res = kill(worker->pid, SIGKILL);
            if (kill_res == -1) {
                if (kill_res == ESRCH) {
                    /* Process no longer exists (it probably
                     * just exited); let waitpid handle it. */
                } else {
                    perror("kill");
                    return false;
                }
            }
        }

        const struct timespec one_msec = { .tv_nsec = 1000000 };
        if (-1 == nanosleep(&one_msec, NULL)) {
            perror("nanosleep");
            return false;
        }
    }
    return true;
}

static enum theft_trial_res
theft_call_inner(struct theft *t, void **args) {
    switch (t->prop.arity) {
    case 1:
        return t->prop.u.fun1(t, args[0]);
        break;
    case 2:
        return t->prop.u.fun2(t, args[0], args[1]);
        break;
    case 3:
        return t->prop.u.fun3(t, args[0], args[1], args[2]);
        break;
    case 4:
        return t->prop.u.fun4(t, args[0], args[1], args[2], args[3]);
        break;
    case 5:
        return t->prop.u.fun5(t, args[0], args[1], args[2], args[3], args[4]);
        break;
    case 6:
        return t->prop.u.fun6(t, args[0], args[1], args[2], args[3], args[4],
            args[5]);
        break;
    case 7:
        return t->prop.u.fun7(t, args[0], args[1], args[2], args[3], args[4],
            args[5], args[6]);
        break;
    /* ... */
    default:
        return THEFT_TRIAL_ERROR;
    }
}

/* Populate a buffer with hashes of all the arguments. */
static void
get_arg_hash_buffer(theft_hash *buffer, struct theft *t) {
    for (uint8_t i = 0; i < t->prop.arity; i++) {
        struct theft_type_info *ti = t->prop.type_info[i];

        theft_hash h = (ti->autoshrink_config.enable
            ? theft_autoshrink_hash(t, t->trial.args[i].instance,
                t->trial.args[i].u.as.env, ti->env)
            : ti->hash(t->trial.args[i].instance, ti->env));

        LOG(4, "%s: arg %d hash; 0x%016" PRIx64 "\n", __func__, i, h);
        buffer[i] = h;
    }
}

/* Check if this combination of argument instances has been called. */
bool theft_call_check_called(struct theft *t) {
    theft_hash buffer[THEFT_MAX_ARITY];
    get_arg_hash_buffer(buffer, t);
    return theft_bloom_check(t->bloom, (uint8_t *)buffer,
        t->prop.arity * sizeof(theft_hash));
}

/* Mark the tuple of argument instances as called in the bloom filter. */
void theft_call_mark_called(struct theft *t) {
    theft_hash buffer[THEFT_MAX_ARITY];
    get_arg_hash_buffer(buffer, t);
    theft_bloom_mark(t->bloom, (uint8_t *)buffer,
        t->prop.arity * sizeof(theft_hash));
}

static enum theft_hook_fork_post_res
run_fork_post_hook(struct theft *t, void **args) {
    if (t->hooks.fork_post == NULL) {
        return THEFT_HOOK_FORK_POST_CONTINUE;
    }

    struct theft_hook_fork_post_info info = {
        .prop_name = t->prop.name,
        .total_trials = t->prop.trial_count,
        .failures = t->counters.fail,
        .run_seed = t->seeds.run_seed,
        .arity = t->prop.arity,
        .args = args,           /* real_args */
    };
    return t->hooks.fork_post(&info, t->hooks.env);
}
