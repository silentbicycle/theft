# theft Changes By Release

## v0.4.5 - 2019-02-11

### API Changes

None.


### Bug Fixes

Only run the prng test suite once. (Probably a merge error.)


### Other Improvements

Changed the pkg-config path setup in the Makefile default to
`${PREFIX}/lib/pkgconfig`, and make it easier to override via
the environment.

Eliminated a couple warnings: an assignment in an assert, missing
prototypes. (Thanks @jmesmon.)

Updated vendored copy of greatest to 1.4.1.


## v0.4.4 - 2018-10-06

### API Changes

None.

### Bug Fixes

Added check so `free` instance callback is optional. Previously, the
documentation indicated it was optional, but the trial cleanup code
always attempted to call it. (Reported by @deweerdt.)

Fixed a bug in `infer_arity` that incorrectly indicated that a
configuration with THEFT_MAX_ARITY (7) arguments had 0. (Reported by
@kquick.)

Fixed a bug in the builtin char array hexdump function's print callback,
which could lead to printing memory past the end of the char array.
(Fixed by @kquick.)


### Other Improvements

Added `-fPIC` to build flags.

Fixed a few typos. (Thanks @neuschaefer.)

Added Makefile targets for vi-style ctags & cscope. (Thanks @alyptik.)

Added `${DESTDIR}` prefix to Makefile install paths, for easier
sandboxed builds and packaging. (Thanks @richardipsum.)


## v0.4.3 - 2017-09-04

### API Changes

Added the `.exit_timeout` field to `struct theft_run_config`'s
`.fork` configuration field. (As this uses the default when 0,
it isn't a breaking API change.)


### Bug Fixes

Fixed worker process management (issue #19): theft now ensures that
forked child processes have terminated and been cleaned up with
`waitpid` before starting another trial, to prevent zombie processes
from accumulating.

Fixed a bug where autoshrinking would find a minimal counter-example,
but then erroneously shrink to a non-minimal one, and subsequently
reject the minimal one as already tried. This was caused by a custom
hash callback that hashed values when autoshrinking -- it would land on
a minimal value (1) right away due to the aux. built-in generator's
special values list, then drop the bits that chose using the special
table, and end up with a larger value. If the special value list input
generating 1 and generating 1 normally hashed differently, then it would
shrink back to 1. This means that providing a custom hash function with
autoshrinking enabled should be an API misuse error, but that is an
interface change, so it will wait until a non-bugfix release.


### Other Improvements

Forked worker processes that have timed out are now given a configurable
window to clean up and exit (possibly successfully) before they are
terminated with SIGKILL.

Moved forked worker process state from local variables to a
`worker_info` structure in `struct theft`. This gathers state that will
later be used to manage multiple workers in parallel (issue #16).

Limited how much the autoshrinker attempts to shrink by dropping
requests when there is a small number of requests -- this reduces
dead ends during shrinking.

Added a `pkg-config` file for libtheft.a. (Thanks @katef.)

Fixed typos in the documentation. (Thanks @katef.)


## v0.4.2 - 2017-08-23

### API Changes

None.


### Bug Fixes

Fixed an autoshrinking bug that could cause shrinking to get stuck on
values close to the actual minimimum.


### Other Improvements

When using the builtin floating point generators, a hexdump of the raw
value is now printed along with the "%g"-formatted output, since it is
lossy.

Autoshrinking instances with many requests is now more efficient.


## v0.4.1 - 2017-08-20

### API Changes

None.


### Bug Fixes

Fixed a possible double-free when a non-first argument's alloc callback
returns SKIP or ERROR.

Fixed a case where the `trial_post` callback could be called with
incorrect pointers in `info->args[]`, due to an inconsistency in how
autoshrink wrapped the arguments and `type_info`. (Issue #18.)

Fixed autoshrink's handling of the `theft_autoshrink_print_mode` default:
now the default is use the `type_info` print callback when defined,
and to otherwise print the requests. (This was the intentended behavior,
but `THEFT_AUTOSHRINK_PRINT_USER` was 0, which meant it was instead
clobbered with `THEFT_AUTOSHRINK_PRINT_REQUESTS`.)


### Other Improvements

Internal refactoring: Autoshrinking is now better integrated into the
argument handling. The bugs addressed in this release came from
inconsistencies in how autoshrink wrapped arguments.


## v0.4.0 - 2017-08-13

### API Changes

Changed the property function typedef (`theft_propfun`). Property
functions are now called with a `struct theft *t` handle as a first
argument -- this can be used to get the hooks' environment with
`theft_hook_get_env` while running the property.

The property function pointer in the `theft_run_config` struct
is now typesafe -- instead of a single function pointer type
with unconstrained arguments, the config struct has fields
`prop1`, `prop2`, ... `prop7`, where the number is the number
of instance arguments the property takes: For example, `prop2`
is:

    enum theft_trial_res
    two_instance_property(struct theft *t, void *arg1, void *arg2);

The property function field has been rename from `fun` to
`prop{ARG_COUNT}`.

Reduced `THEFT_MAX_ARITY` to 7.

Added the `.fork` structure to `struct theft_run_config` -- if
`.fork.enable` is set, then theft will fork before running the property
function. This can be used to shrink input that causes the code under
test to crash. If forking is enabled, `.fork.timeout` adds a timeout (in
milliseconds) for each property trial, to shrink input that causes
infinite loops or wide runtime variation. `.fork.signal` customizes
the signal sent on timeout. See `doc/forking.md` for details.

Added a `fork_post` hook, which is called on the child process
after forking. This can be used to drop privileges before
running the property test.

Added `theft_generate`, to generate and print an instance based
on a given seed (without running any properties).

Manual Bloom filter configuration is deprecated, because the Bloom
filter now resizes automatically -- The bloom_bits setting in
`struct theft_run_config` and related constants are ignored,
and will be removed in a future release.

Added `theft_random_choice`, which returns approximately evenly
distributed random `uint64_t` values less than an upper bound.

Added `theft_run_res_str`, which returns a string (e.g. "PASS") for an
`enum theft_run_res` value.

Removed `THEFT_RUN_ERROR_MISSING_CALLBACK` from `enum theft_run_res`;
it's now combined with `THEFT_RUN_ERROR_BAD_ARGS`.

Added `THEFT_RUN_ERROR_MEMORY` to `enum theft_run_res`. This is
returned if internal memory allocation fails.

Added `repeat` flag to the info struct associated with the
`trial_post` hook. This is set when a test is being repeated.

Added `theft_hook_first_fail_halt`, a `trial_pre` hook that halts after
the first failure.


### Other Improvements

Switch to a dynamic blocked Bloom filter instead of a fixed-size Bloom
filter. This makes manual filter size configuration unnecessary, and
significantly reduces theft's memory overhead -- instead of a single
large Bloom filter, it now uses a set of small filters, which can
individually grow as necessary.

Lots of internal refactoring.

Added a warning when the `trial_done` callback is overridden
but `theft_print_trial_result` is called with the overall
hook environment pointer (cast to a `theft_print_trial_result_env`),
since this is probably API misuse.


## v0.3.0 - 2017-06-15

### API Changes

There is now support for generic shrinking. To use this, set
`.autoshrink_config.enable` to `true` on the `theft_type_info`
struct. This requires the `theft_alloc_cb` to be written so
`theft_random_bits` giving smaller random values will lead to
simpler generated instances -- see `doc/shrinking.md`. This
feature is still experimental.

`theft_init` and `theft_free` have been removed from the public API, and
`theft_run` no longer has a first argument of a `struct theft *t`
handle. Instead, the theft test runner is allocated and freed inside of
`theft_run`, reducing boilerplate.

The `theft_progress_cb` callback's role has significantly expanded. It
has been broken up into several distinct callbacks, which are set within
`theft_run_config`'s `hooks` struct. See the *Hooks* section in
`doc/usage.md` and their individual type definitions for more
information. These hooks support many useful test-specific behaviors,
such as halting shrinking after a time limit or a certain number of
unsuccessful shrinks, or re-running a failed trial (with arguments
shrunken to a local minima) with log levels adjusted or a debugger
attached.

`struct theft_type_info` now has a `void *env` field, and that (rather
than the `void *env` associated with hooks) will be passed to its
callbacks. It can be NULL, or the same as the hooks' environment.
This allows type-specific details, such as limits, to be passed to the
callbacks, and also makes reuse of the `type_info` callbacks easier.

Added `theft_random_bits`, which returns less than the full 64 bits from
the random number generator, and buffers the rest for future requests.
`theft_random_bits_bulk` can be used to request more 64 bits at once, as
long as a buffer is provided. (`theft_random` can still be used to get
64 bits at a time, but will be removed in a future release.)

The `theft_alloc_cb` callback no longer has a random seed passed to it
directly. Instead, `theft_random_bits(t, bit_count)` or
`theft_random_bits_bulk(t, bit_count, buffer)` should be used. This
tells theft how much of the random bit stream is being consumed, which
improves efficiency and influences several internal heuristics.

The `theft_shrink_cb` callback now has the `struct theft *t` handle
passed to it as an extra argument -- this is so shrink callbacks
can use its random number generator.

Some arguments to the following functions have been made `const`:
    - theft_run
    - theft_print_cb
    - theft_hash_cb
    - theft_hash_onepass
    - theft_hash_sink

The following enum types in the API are no longer typedef'd:
    - enum theft_trial_res
    - enum theft_run_res
    - enum theft_progress_callback_res

Rather than returning a void pointer or special sentinel values (e.g.
`THEFT_DEAD_END`), the `alloc` and `shrink` callbacks now return an enum
and (when appropriate) write their output into a pointer argument called
`output`.

The instance argument to the `shrink` callback is now const.

Renamed `struct theft_cfg` to `struct theft_run_config`.

The `struct theft` type is now opaque.

`THEFT_BLOOM_DISABLE` has been removed -- the Bloom filter
is always allocated now.

In `struct theft_run_config`, `always_seed_count` and `trials` are now
`size_t`s rather than `int`s.

The struct `theft_run_config`'s type_info array field now points
to `const` values, because the built-in type_info structs returned
by `theft_get_builtin_type_info` are const.

`theft_run` will now return `THEFT_RUN_SKIP` if a run completes
without any passes or failures (because all trials were skipped).

The output format for properties and counter-examples has been
streamlined.

Added `theft_seed_of_time`, to get a PRNG seed based on the current
time.

Added `theft_generic_free_cb`, a `free` type_info callback that just
calls `free(instance)`.

Added `theft_trial_res_str`, which returns a string (e.g. "PASS") for an
`enum theft_trial_res` value.

Added several built-in generators for common types, which can be
accessed via `theft_get_builtin_type_info` (to use them as-is) or
`theft_copy_builtin_type_info` (to copy them, and then set a limit).


### Other Improvements

Added this changelog.

Added documentation under `doc/`, and updated the `README.md`.

Added improved syntax highlighting to the README. (Thanks @lochsh).

Added `CONTRIBUTING.md`.

Fixed bug in error checking code path. (Thanks @iximeow.)

Updated vendored version of greatest.

Use inttypes.h and `PRIx64` to avoid printf string build warning.

Restructured project layout to build in a `build` directory, and
keep source, header, and test files in `src`, `inc`, and `test`.

Broke up `theft.c` into several files.

An intentionally tautological comparison in a test has been removed,
because it led to warnings at build-time and suppressing the warning
wasn't portable between compilers.

Added Makefile targets for coverage checking and profiling.


## v0.2.0 - 2014-08-06

### API Changes

Add `THEFT_BLOOM_DISABLE` to explicitly disable Bloom filter.

Switch to 64-bit Mersenne Twister.


### Other Improvements

README and documentation changes.



## v0.1.0 - 2014-06-29

Initial public release.
	
	

