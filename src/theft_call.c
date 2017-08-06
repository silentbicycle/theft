#include "theft_call_internal.h"

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
    struct theft_run_info *run_info = t->run_info;
    assert(run_info);
    enum theft_trial_res res = THEFT_TRIAL_ERROR;

    if (t->fork.enable) {
        struct timespec tv = { .tv_nsec = 1 };
        int fds[2];
        if (-1 == pipe(fds)) {
            return THEFT_TRIAL_ERROR;
        }
        pid_t pid = -1;
        for (;;) {
            pid = fork();
            if (pid == -1) {
                if (errno == EAGAIN) {
                    /* If we get EAGAIN, then wait for terminated
                     * child processes a chance to clean up -- forking
                     * is probably failing due to RLIMIT_NPROC. */
                    const int pre_sleep_errno = errno;
                    int stat_loc = 0;
                    (void)waitpid(-1, &stat_loc, WNOHANG);
                    if (-1 == nanosleep(&tv, NULL)) {
                        perror("nanosleep");
                        return THEFT_TRIAL_ERROR;
                    }
                    if (tv.tv_nsec >= (1L << MAX_FORK_RETRIES)) {
                        errno = pre_sleep_errno;
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
            close(fds[0]);
            close(fds[1]);
            return THEFT_TRIAL_ERROR;
        } else if (pid == 0) {  /* child */
            close(fds[0]);
            res = theft_call_inner(t, args);
            uint8_t byte = (uint8_t)res;
            ssize_t wr = write(fds[1], (const void *)&byte, sizeof(byte));
            exit(wr == 1 && res == THEFT_TRIAL_PASS
                ? EXIT_SUCCESS
                : EXIT_FAILURE);
        } else {                /* parent */
            close(fds[1]);
            res = parent_handle_child_call(t, pid, fds[0]);
            int stat_loc = 0;
            pid_t wait_res = waitpid(pid, &stat_loc, WNOHANG);
            LOG(2 - LOG_CALL, "%s: waitpid %d ? %d\n",
                __func__, pid, wait_res);
            (void)wait_res;
            close(fds[0]);
            return res;
        }
    } else {                    /* just call */
        res = theft_call_inner(t, args);
    }
    return res;
}

static enum theft_trial_res
parent_handle_child_call(struct theft *t, pid_t pid, int fd) {
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
        int stat_loc = 0;
        pid_t wait_res = waitpid(pid, &stat_loc, 0);
        LOG(2 - LOG_CALL, "%s: kill waitpid: %d ? %d\n",
            __func__, pid, wait_res);
        assert(wait_res == pid);
        /* If the child called exit(EXIT_SUCCESS) then
         * consider it a PASS, even though it timed out. */
        LOG(2 - LOG_CALL, "exited? %d, exit_status %d\n",
            WIFEXITED(stat_loc), WEXITSTATUS(stat_loc));
        if (WIFEXITED(stat_loc) && WEXITSTATUS(stat_loc) == EXIT_SUCCESS) {
            return THEFT_TRIAL_PASS;
        } else {
            return THEFT_TRIAL_FAIL;
        }
    } else {
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
            return THEFT_TRIAL_FAIL;
        } else {
            assert(rd == 1);
            return (enum theft_trial_res)res_byte;
        }
    }
}

static enum theft_trial_res
theft_call_inner(struct theft *t, void **args) {
    struct theft_run_info *run_info = t->run_info;
    switch (run_info->arity) {
    case 1:
        return run_info->fun(t, args[0]);
        break;
    case 2:
        return run_info->fun(t, args[0], args[1]);
        break;
    case 3:
        return run_info->fun(t, args[0], args[1], args[2]);
        break;
    case 4:
        return run_info->fun(t, args[0], args[1], args[2], args[3]);
        break;
    case 5:
        return run_info->fun(t, args[0], args[1], args[2], args[3], args[4]);
        break;
    case 6:
        return run_info->fun(t, args[0], args[1], args[2], args[3], args[4],
            args[5]);
        break;
    case 7:
        return run_info->fun(t, args[0], args[1], args[2], args[3], args[4],
            args[5], args[6]);
        break;
    case 8:
        return run_info->fun(t, args[0], args[1], args[2], args[3], args[4],
            args[5], args[6], args[7]);
        break;
    case 9:
        return run_info->fun(t, args[0], args[1], args[2], args[3], args[4],
            args[5], args[6], args[7], args[8]);
        break;
    case 10:
        return run_info->fun(t, args[0], args[1], args[2], args[3], args[4],
            args[5], args[6], args[7], args[8], args[9]);
        break;
    /* ... */
    default:
        return THEFT_TRIAL_ERROR;
    }
}

/* Populate a buffer with hashes of all the arguments. */
static void
get_arg_hash_buffer(theft_hash *buffer,
        struct theft_run_info *run_info, void **args) {
    for (uint8_t i = 0; i < run_info->arity; i++) {
        struct theft_type_info *ti = run_info->type_info[i];
        theft_hash h = ti->hash(args[i], ti->env);
        LOG(4, "%s: arg %d hash; 0x%016" PRIx64 "\n", __func__, i, h);
        buffer[i] = h;
    }
}

/* Check if this combination of argument instances has been called. */
bool theft_call_check_called(struct theft *t, void **args) {
    struct theft_run_info *run_info = t->run_info;
    theft_hash buffer[THEFT_MAX_ARITY];
    get_arg_hash_buffer(buffer, run_info, args);
    return theft_bloom_check(t->bloom, (uint8_t *)buffer,
        run_info->arity * sizeof(theft_hash));
}

/* Mark the tuple of argument instances as called in the bloom filter. */
void theft_call_mark_called(struct theft *t, void **args) {
    struct theft_run_info *run_info = t->run_info;
    theft_hash buffer[THEFT_MAX_ARITY];
    get_arg_hash_buffer(buffer, run_info, args);
    theft_bloom_mark(t->bloom, (uint8_t *)buffer,
        run_info->arity * sizeof(theft_hash));
}
