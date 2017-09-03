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

Along with the original input, the shrink callback gets a "tactic"
argument -- this is used to choose between different ways to simplify
the instance: "Try to simplify using tactic #1...try to simplify using
tactic #2...". These should be ordered so tactics that remove more
information are tried first, because shrinking by bigger steps helps
theft to converge faster on minimal counter-examples. Successful
shrinking steps reset the tactic counter to 0, in case a later tactic
got earlier tactics unstuck.

For a list of numbers, shrinking tactics might include:

+ Discarding some percent of the list at random
+ Discarding a smaller percent of the list at random
+ Dividing all the numbers by 2 (with integer truncation)
+ Discarding a single value, based on the tactic ID
+ Dividing a single value by 2
+ Subtracting 1 from a single value

The first three tactics shrink the list by much larger steps than the
others. The later tactics will only be tried once the first three aren't
able to make any more progress (because any changes make the test start
passing), but the later tactics' changes may get the first three
unstuck. Shrinking continues until no tactics can make any more
progress, or a custom hook halts shrinking early.

The shrink callback can vary its tactics as the instance changes. For
example, exploring changes to every individual byte in a 64 KB byte
buffer could take too long, but could be worth trying once other tactics
have reduced the buffer to under 1 KB. Attempting to individually
discard every entry in a list takes `O(n!)` time due to backtracking,
but randomly discarding entries with a 1/32 chance can quickly shrink
the list without a runtime explosion. Switching to discarding every
individual entry may be fine once the list is fairly small (under 100
entries, perhaps). Since the shrink callback's tactic argument is just
an integer, its interpretation is deliberately open-ended.

theft assumes that that the same combination of instance, `env` info,
and shrinking tactic number should always simplify to the same new
instance, and therefore lead to the property function having the same
result.


## Auto-shrinking

As of version 0.3.0, theft has experimental support for auto-shrinking.
Instead of using a custom `shrink` callback, it can instead save the
random bitstream used to generate the argument instance, and re-run the
property test while simplifying the bitstream. This implementation is
loosely based on a design described by David R. MacIver.

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

To enable auto-shrinking, set:

```c
    .autoshrink_config = {
        .enabled = true,
    },
```

in the `theft_type_info` struct. There are other optional configuration
fields in this struct -- see the definition for `struct
theft_autoshrink_config` in `inc/theft_types.h`.

If autoshrinking is enabled, the `theft_type_info` struct should not
have `shrink` or `hash` callbacks defined. If a custom `print` callback
it provided, it will always be run. Otherwise, default behavior will be
provided for `print` (print the bit pool's requests).

The `print` behavior can be configured via the `print_mode` field:

- `THEFT_AUTOSHRINK_PRINT_USER`: Only run the custom `print` callback.

- `THEFT_AUTOSHRINK_PRINT_BIT_POOL`: Print the raw bit pool.

- `THEFT_AUTOSHRINK_PRINT_REQUESTS`: Print the request sizes and the
  values that were returned. This is the default.

- `THEFT_AUTOSHRINK_ALL`: Print the raw bit pool and requests.

