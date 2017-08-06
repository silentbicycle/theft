# theft Changes By Release

## v0.4.0 - TBD

### API Changes

Added the `.fork` structure to `struct theft_run_config` -- if
`.fork.enable` is set, then theft will fork before running the property
function. This can be used to shrink input that causes the code under
test to crash. If forking is enabled, `.fork.timeout` adds a timeout (in
milliseconds) for each property trial, to shrink input that causes
infinite loops or wide runtime variation. `.fork.signal` customizes
the signal sent on timeout. See `doc/forking.md` for details.

Manual Bloom filter configuration is deprecated, because the Bloom
filter now resizes automatically -- The bloom_bits setting in
`struct theft_run_config` and related constants are ignored,
and will be removed in a future release.


Added `theft_random_choice`, which can be used to get an approximately
evenly distributed random `uint64_t` values less than an upper bound.

Added `theft_run_res_str`, which returns a string (e.g. "PASS") for an
`enum theft_run_res` value.

Removed `THEFT_RUN_ERROR_MISSING_CALLBACK` from `enum theft_run_res`;
it's now combined with `THEFT_RUN_ERROR_BAD_ARGS`.

Added `THEFT_RUN_ERROR_MEMORY` to `enum theft_run_res`. This is
returned if internal memory allocation fails.

Added `repeat` flag to the info struct associated with the
`trial_post` hook. This is set when a test is being repeated.


### Other Improvements

Switch to a dynamic blocked Bloom filter instead of a fixed-size Bloom
filter. This makes manual filter size configuration unnecessary, and
significantly reduces theft's memory overhead -- instead of a single
large Bloom filter, it now uses a set of small filters, which can
individually grow as necessary.


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
	
	

