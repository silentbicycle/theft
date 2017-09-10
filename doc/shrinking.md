# Shrinking

Once theft finds input that makes a property fail, it attempts to
"shrink" it to a minimal example. While it can be hard to tell what
aspect of the original generated input caused the property to fail,
shrinking gradually eliminates irrelevant details, leaving input that
should more clearly point to the root cause.

Each shrinking step produces a simpler copy of the input, runs the test
again, and checks if it still failed. If so, it commits to that version
and continues shrinking. If the test passes, then it reverts to the
previous version and tries to simplify it another way (or switches
between multiple arguments). It also tracks which combinations of
arguments have been tried, so it doesn't run the test again with the
same input. When shrinking can't make any more progress, it reports the
input as the simplest counter-example it could find.


## Auto-shrinking

In version 0.3.0, theft gained support for auto-shrinking. Instead of
using a custom `shrink` callback, it can instead save the random
bitstream used to generate the argument instance, and re-run the
property test while simplifying the bitstream. This implementation is
loosely based on a design described by David R. MacIver. In version
0.5.0, this became the default, and custom `shrink` callbacks were
removed form the API.

In order for auto-shrinking to work, the `alloc` callback has an
additional requirement -- when `theft_random_bits` return smaller
values, it should lead to a simpler generated result. In particular, a
bitstream of all `0` bits should produce a minimal value for the type.
As long as that holds, theft can modify the bitstream in ways that
gradually reduce the complexity of the instance, with heuristics
specific to shrinking a random bit pool.

While theft doesn't know anything about the type that the user's `alloc`
callback generates, the random bit request sizes give it some clues
about the structure -- these correspond to decisions that influence the
generated type somehow. Modifications to the bit pool during
auto-shrinking are aligned to those requests' bits.

If a custom `print` callback is not provided, autoshrinking will
default to printing the bit pool's contents, broken up into the
values given to individual requests.

The `print` behavior can be configured via the
`autoshrink_config.print_mode` field:

- `THEFT_AUTOSHRINK_PRINT_USER`: Only run a custom `print` callback.

- `THEFT_AUTOSHRINK_PRINT_BIT_POOL`: Print the raw bit pool.

- `THEFT_AUTOSHRINK_PRINT_REQUESTS`: Print the request sizes and the
  values that were returned. This is the default.

- `THEFT_AUTOSHRINK_ALL`: Print the raw bit pool and requests.
