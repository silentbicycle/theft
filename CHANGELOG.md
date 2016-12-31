# theft Changes By Release

## v0.3.0 - TBD

### API Changes

Added `theft_get_bits()`.

The following enum types in the API are no longer typedef'd:
    - enum theft_trial_res
    - enum theft_run_res
    - enum theft_progress_callback_res


### Other Improvements

Added this changelog.

Fix bug in error checking code path. (Thanks iximeow.)

Update vendored version of greatest.

Use inttypes.h and `PRIx64` to avoid printf string build warning.

Restructured project layout to build in a `build` directory, and
keep source, header, and test files in `src`, `inc`, and `test`.


## v0.2.0 - 2014-08-06

### API Changes

Add `THEFT_BLOOM_DISABLE` to explicitly disable bloom filter.

Switch to 64-bit Mersenne Twister.


### Other Improvements

README and documentation changes.



## v0.1.0 - 2014-06-29

Initial public release.
	
	

