# theft Changes By Release

## v0.3.0 - TBD

### API Changes

There is now support for generic shrinking. To use this, set
`.autoshrink_config.enable = true` on the `theft_type_info`
struct. This requires the `theft_alloc_cb` to be written so
`theft_random_bits()` giving smaller random values will
generate simpler instances.

`theft_init` and `theft_free` have been removed from the public API, and
`theft_run` no longer has a first argument of a `struct theft *t`
handle. Instead, the theft test runner is allocated and freed inside of
`theft_run`, reducing boilerplate.

The `theft_progress_cb` callback's role has significantly expanded.
Rename it and its related types to `theft_hook_cb` throughout.
Instead of only being called with the result after each trial, it is
called in several contexts, and is passed a tagged union with details
specific to that context. This hook supports many useful test-specific
behaviors, such as halting shrinking after a time limit or a certain
number of unsuccessful shrinks, or re-running a failed trial (with
arguments shrunken to a local minima) after adjusting logging or adding
breakpoints.

`struct theft_type_info` now has a `void *env` field, and that
(rather than the `void *env` associated with the `theft_hook_cb`)
will be passed to its callbacks. It can be NULL, or the same as the
`hook_cb`'s environment. This allows type-specific details, such as
limits, to be passed to the callbacks. This makes reuse of the
`type_info` callbacks easier.

Added `theft_random_bits()`, which returns less than the full 64 bits
from the random number generator, and buffers the rest for future
requests. (`theft_random()` can still be used to get 64 bits at a
time.)

The `theft_alloc_cb` callback no longer has a random seed passed to it
directly. Instead, `theft_random(t)` or `theft_random_bits(t, BITS)`
should be used. This tells theft how much of the random bit stream is
being consumed.

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

Rather than returning a void pointer or special sentinel values
(e.g. `THEFT_DEAD_END`), the alloc and shrink callbacks now return
an enum and (when appropriate) write their output into a pointer
argument called `output`.

The instance argument to the shrink callback is now const.

Renamed `struct theft_cfg` to `struct theft_run_config`.

The `struct theft` type is now opaque.

`THEFT_BLOOM_DISABLE` has been removed -- the bloom filter
is always allocated now.

`theft_init` now takes a pointer to a `struct theft_config` with
configuration, rather than just a number of bits to use for the
bloom filter. If given a NULL pointer, it will use defaults.

In the config, `always_seed_count` and `trials` are now `size_t`s
rather than `int`s.

The struct `theft_run_config`'s type_info array field now points
to `const` values, because the built-in type_info structs returned
by `theft_get_builtin_type_info` are const.


### Other Improvements

Added this changelog.

Fix bug in error checking code path. (Thanks iximeow.)

Update vendored version of greatest.

Use inttypes.h and `PRIx64` to avoid printf string build warning.

Restructured project layout to build in a `build` directory, and
keep source, header, and test files in `src`, `inc`, and `test`.

Broke up `theft.c` into several files.


## v0.2.0 - 2014-08-06

### API Changes

Add `THEFT_BLOOM_DISABLE` to explicitly disable bloom filter.

Switch to 64-bit Mersenne Twister.


### Other Improvements

README and documentation changes.



## v0.1.0 - 2014-06-29

Initial public release.
	
	

