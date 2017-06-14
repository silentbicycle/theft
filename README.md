# theft: property-based testing for C

theft is a C library for property-based testing. Rather than checking
test results for specific input, general properties are asserted ("for
any possible input, [some condition] should hold"), and theft generates
input and searches for counter-examples. If it finds arguments that make
the test fail, it also knows how to search for progressively simpler
failing input, and ultimately reports minimal steps to reproduce the
failure.

theft is distributed under the ISC license.


## Installation & Dependencies

theft does not depend on anything beyond C99 and a Unix-like
environment. Its internal tests use [greatest][], but there is not any
coupling between them. It contains implementations of the
[Mersenne Twister][mt] PRNG and the [FNV-1a][fnv] hashing algorithm -
see their files for copyright info.

[greatest]: https://github.com/silentbicycle/greatest
[mt]: http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html
[fnv]: http://www.isthe.com/chongo/tech/comp/fnv/


To build, using GNU make:

    $ make

Note: You may need to call it as `gmake`, especially if building on BSD.

To build and run the tests:

    $ make test

This will produce example output from several falsifiable properties,
and confirm that failures have been found.

To install libtheft and its headers:

    $ make install    # using sudo, if necessary

theft can also be vendored inside of projects -- in that case, just make
sure the headers in `${VENDOR}/theft/inc/` are added to the `-I` include
path, and `${VENDOR}/theft/build/libtheft.a` is linked.


## Usage

For usage documentation, see [doc/usage.md](blob/master/doc/usage.md).


## Properties

For some examples of properties to test, see
[doc/properties.md](blob/master/doc/properties.md).


## Shrinking and Auto-shrinking

For more info about shrinking and auto-shrinking, see
[doc/shrinking.md](blob/master/doc/shrinking.md).
