# theft Changes By Release

## v0.3.0 - TBD

### API Changes

Added `theft_random_bits()`, which returns less than the full 64 bits
from the random number generator, and buffers the rest for future
requests. This also tells theft how much of the random bit stream is
being used. (`theft_random()` can still be used to get 64 bits at a
time.)

The following enum types in the API are no longer typedef'd:
    - enum theft_trial_res
    - enum theft_run_res
    - enum theft_progress_callback_res

Rather than returning a void pointer or special sentinel values
(e.g. `THEFT_DEAD_END`), the alloc and shrink callbacks now return
an enum and (when appropriate) write their output into a pointer
argument called `output`.

The instance argument to the shrink callback is now const.

Renamed `struct theft_cfg` to `struct theft_config`.


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
	
	

