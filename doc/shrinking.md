# Shrinking

Once theft has found input that causes the property to fail, it will try
to 'shrink' it to a minimal example. It can be hard to tell what aspect
of the original random arguments caused the property to fail, but
shrinking will gradually eliminate irrelevant details, leaving input
that should point directly at the problem. (These simplified arguments
may also be good test data for unit/regression tests.)

The shrink callback is given a tactic argument, which chooses between
ways to simplify the instance: "Try to simplify this using tactic #2".
These should be ordered by how much they simplify the instance, because
shrinking by bigger steps helps theft to converge faster on minimal
counter-examples. Any successful shrink will reset the tactic counter
to 0, in case later tactics get the earlier tactics unstuck.

For a list of numbers, shrinking tactics could include:

+ Discarding the first half of the list
+ Discarding the second half of the list
+ Dividing all the numbers by 2 (with integer truncation)
+ Discarding a single value, based on the tactic ID

The first 3 shrink the list by much larger steps than the others, which
will only be tried once first 3 discard whatever details are causing the
property to fail. Then, if a later tactic leads to a simpler failing
instance, then it will try the earlier tactics again in the next pass --
they may no longer lead to dead ends.

Shrinking works by breadth-first search over all arguments and all of
their shrinking tactics, so it will attempt to simplify all arguments
that have shrinking behavior specified. While this tends to find local
minima rather than the absolute simplest counter-examples, it will
always report the simplest counter-examples it finds. If hashing
callbacks are provided, it will avoid revisiting parts of the state
space that it has already tested.

The shrink callback can also vary its tactics as the instance changes.
For example, exploring changes to every individual byte in a 64 KB byte
buffer is probably too expensive, but could be worth trying once other
tactics have reduced the buffer to under 1 KB. Attempting to
individually discard every entry in a list will be `O(n!)` (due to
backtracking), but randomly discarding every entry with 1/32 odds each
pass through can quickly shrink the list without a runtime explosion.
Attempting to discard every individual entry may be fine once the list
is fairly small (10-100 entries, perhaps). Since the shrink callback's
tactic argument is just an integer, its interpretation is deliberately
open-ended.

The requirement for shrinking is that the same combination of `env`
info, argument(s), and shrinking tactic number should always simplify to
the same instance, and therefore lead to the property function having
the same result.


## Auto-shrinking

As of v. 0.3.0, theft has experimental support for auto-shrinking.
Instead of using a custom `shrink` callback, it can instead save the
random bitstream used to generate the argument instance, and re-run the
property test while simplifying the bitstream. (This implementation is
loosely based on a design described by David. R. MacIver).

In order to use auto-shrinking, the `alloc` callback needs to have an
additional constraint -- getting smaller values from `theft_random_bits`
should lead to simpler instances being generated. In particular, a
bitstream of all `0` bits should produce a minimal value for the type.
As long as that holds, theft can modify the bitstream in ways that
gradually reduce the complexity of the instance, with heuristics
specific to shrinking a random bit pool.

While theft doesn't know anything about the type that the user's
`alloc` callback is generating, it knows something about the
structure -- the random bit request sizes. These correspond to
the generated type somehow, so modifications to the bit pool
are aligned to the bits returned in those requests.

To enable auto-shrinking, set:

    .autoshrink_config = {
        .enabled = true,
    },

in the `theft_type_info` struct. There are other optional configuration
fields in this struct -- see the definition for `struct
theft_autoshrink_config`.

If autoshrinking is enabled, the `theft_type_info` struct should not
have a `shrink` callback defined, and default behavior will also be
provided for `hash` (hashing the portion of bit pool that is used) and
`print` (the bit pool, broken up into its request sizes).
