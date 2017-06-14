# theft glossary

## counter-example

A particurlar combination of one or more instance(s) that
cause a property test function to fail (i.e., the property
does not hold for that input).

## dup (duplicate)

A duplicate trial (one whose combination of input instances have
already been tried).

## env (environment)

A void pointer with arbitrary context for the user's callbacks.
The env pointer is passed along, but opaque to theft itself.

## hash

A mathematical fingerprint derived from some data. theft uses
hashes to check whether a particular combination of instances
has already been tested (via a bloom filter).

Also, the act of computing the hash of some data.

## instance

A specific value, generated from a known random number generator
seed and a type-specific `alloc` function.

## property

A test function that is expected to hold (that is, return
`THEFT_TRIAL_PASS`) for arbitrary generated input instance(s). If any
input causes the property to fail (return `THEFT_TRIAL_FAIL`), then a
counter-example to the property has been found -- theft will attempt to
shrink the instance(s) as much as possible before reporting the
counter-example.

## run

A batch of trials (typically 100), checking a property test
function with a variety of input instances.

## seed

a starting state for the random number generator.

## shrink

Taking an instance and returning a simpler copy, or indicating
that there are no ways to simplify the instance. When there are
multiple ways to shrink the instance, a 'tactic' is used to
decide between them.

## tactic

A numerical ID used to choose between multiple ways to shrink
(simplify) an instance.

## trial

A single test, checking if a property function holds for a single
instance of each input argument.
