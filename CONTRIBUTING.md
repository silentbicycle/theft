# Contributing to theft

Thanks for taking time to contribute to theft!

Some issues may be tagged with `beginner` in [the Issues page][issues],
those should be particularly approachable.

Please send patches or pull requests against the `develop` branch. This
makes it easier to avoid interface changes until they can be reflected
in version number updates and the CHANGELOG.

Sending changes via patch or pull request acknowledges that you are
willing and able to contribute it under this project's license. (Please
don't contribute code you aren't legally able to share.)


## Bug Reports

Please report bugs at [the Issues page][issues].

[issues]: https://github.com/silentbicycle/theft/issues

If you are reporting a bug, please include:

+ Your operating system name and version.

+ Your compiler version and target platform.

+ Any details about your local setup that might be helpful in
  troubleshooting.

+ Detailed steps to reproduce the bug.


## Documentation

Improvements to the documentation are welcome. So are requests for
clarification -- if the documentation or comments in the API headers
are unclear or misleading, that's a potential source of bugs.


## Versioning & Compatibility

The versioning format is MAJOR.MINOR.PATCH.

Performance improvements or minor bug fixes that do not break
compatibility with past releases lead to patch version increases. API
changes that do not break compatibility lead to minor version increases
and reset the patch version. Changes that do break compatibility
will lead to a major version increase once reaching version 1.0.0, but
will lead to a minor version increase until then. All breaking changes
should between releases should be noted in the CHANGELOG.

Values derived from the PRNG bitstream are not expected to be consistent
between versions of the library, though it would be better to stay
consistent when possible. (This may change after reaching version
1.0.0.)


## Portability

theft expects to run in a Unix-like environment. It is currently tested
on Linux (64-bit `x86_64` and 32-bit ARM), OpenBSD (`amd64`), and OSX, and
running it on other OSs and hardware platforms may help discover bugs.

Aside from that, theft tries to assume little about its environment, and
tries to avoid depending on external libraries.

It may run on Windows eventually, but I haven't put any effort into that
yet.


## Testing

The internal tests are based on [greatest][g]. The test suite is expected
to run without any warnings from `valgrind` (aside from the warnings
about still reachable `fprintf` buffers on OSX).

[g]: https://github.com/silentbicycle/greatest

Note that integration tests may fail a very small percentage of the
time, because many of the tests are probabalistic -- they check whether
theft was able to find a known minimal case within a certain number of
tries (and getting close, but not to the exact value, is still a
failure). They could be fixed by always using the same seeds, but
currently I am using this to motivate improving autoshrinking further,
and eventually this problem should go away entirely.

Contributors are encouraged to add tests for any new functionality, and
in particular to add regression tests for any bugs found.
