# Forking

theft can optionally fork and run the property function in a child process,
so that it can shrink failures that cause crashes or infinite loops.

Forking is configured via the `.fork` struct nested inside of
`struct theft_run_config`:

```c
    .fork = {
        .enable = true,      /* default: disabled */
        .timeout = TIMEOUT_IN_MSEC,  /* default: 0 (no timeout) */
        .signal = SIGTERM,   /* default: SIGTERM */
    },
```

Note that changes to memory in the child process will not be
visible to the parent process, due to copy-on-write.


## Performance

The overhead of shrinking a repeatedly crashing failure can vary
significantly between operating systems.

In particular, the CrashReporter on macOS slows down shrinking by
several orders of magnitude, as it logs information for every single
crashing process. [Disabling the CrashReporter][1] or running theft
on a non-macOS virtual machine can improve performance.

[1]: https://www.gregoryvarghese.com/reportcrash-high-cpu-disable-reportcrash/


## Timeouts

If forking is enabled, the `.timeout` field can be used to configure a
timeout for each property trial (in milliseconds). If `.timeout` is
nonzero, theft will `kill(2)` the child process if the test does not
complete within the timeout. The `kill` signal defaults to `SIGTERM`,
but can me configured via the `.signal` field.

After sending the signal, theft will wait for the child process to exit.
If the test needs custom cleanup code, then send a signal such as
`SIGUSR1` to the test instead. Early in the property function, register
a signal handler that will clean up and exit.

If the child process handles the signal and then returns
`THEFT_TRIAL_PASS` or calls `exit(EXIT_SUCCESS)`, then the trial will
still be considered a `PASS`, otherwise the trial will be considered a
`FAIL`. Signals that kill the process (such as `SIGTERM` or `SIGKILL`)
will always be considered a `FAIL`.
