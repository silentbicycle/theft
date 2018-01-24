# Usage

First, `#include "theft.h"` in your code that uses theft. (It will
include `theft_types.h` internally -- no need to include that directly.)

Then, define a property function:

```c
    static enum theft_trial_res
    prop_encoded_and_decoded_data_should_match(struct theft *t, void *arg1) {
        struct buffer *input = (struct buffer *)arg1;
        // [compress & uncompress input, compare output & original input]
        // return THEFT_TRIAL_PASS, FAIL, SKIP, or ERROR
    }
```

This should take one or more generated arguments and return
`THEFT_TRIAL_PASS`, `THEFT_TRIAL_FAIL` if a counter-example to the
property was found, `THEFT_TRIAL_SKIP` if the combination of argument(s)
should be skipped, or `THEFT_TRIAL_ERROR` if the whole theft run should
halt and return an error.

Then, define how to generate the input argument(s) by providing a struct
with callbacks. (This definition can be shared between properties.)

For example:

```c
    static struct theft_type_info random_buffer_info = {
        /* allocate a buffer based on random bitstream */
        .alloc = random_buffer_alloc_cb,
        /* free the buffer */
        .free = random_buffer_free_cb,
        /* get a hash based on the buffer */
        .hash = random_buffer_hash_cb,
        /* return a simpler variant of a buffer, or an error */
        .shrink = random_buffer_shrink_cb,
        /* print an instance */
        .print = random_buffer_print_cb,
    };
```

All of these callbacks except 'alloc' are optional. For more details,
see the **Type Info Callbacks** subsection below.

If *autoshrinking* is used, type-generic shrinking and hashing
can be handled internally:

```c
    static struct theft_type_info random_buffer_info = {
        .alloc = random_buffer_alloc_cb,
        .free = random_buffer_free_cb,
        .print = random_buffer_print_cb,
        .autoshrink_config = {
            .enable = true,
        },
    };
```

Note that this has implications for how the `alloc` callback is written.
For details, see "Auto-shrinking" in [shrinking.md](shrinking.md).

Finally, call `theft_run` with a configuration struct:

```c
    bool test_encode_decode_roundtrip(void) {
        struct repeat_once_env env = { .fail = false };

        /* Get a seed based on the current time */
        theft_seed seed = theft_seed_of_time();

        /* Property test configuration.
         * Note that the number of type_info struct pointers in
         * the .type_info field MUST match the field number
         * for the property function (here, prop1). */
        struct theft_run_config config = {
            .name = __func__,
            .prop1 = prop_encoded_and_decoded_data_should_match,
            .type_info = { &random_buffer_info },
            .seed = seed,
        };

        /* Run the property test. */
        enum theft_run_res res = theft_run(&config);
        return res == THEFT_RUN_PASS;
    }
```

The return value will indicate whether it was able to find any failures.

The config struct has several optional fields. The most commonly
customized ones are:

- trials: How many trials to run (default: 100).

- seed: The seed for the randomly generated input.

- hooks: There are several hooks that can be used to control the test
  runner behavior -- see the **Hooks** subsection below.

- fork: For details about forking, see [forking.md](forking.md).


## Type Info Callbacks

All of the callbacks are passed the `void *env` field from their
`theft_type_info` struct. This pointer is completely opaque to theft,
but can be cast to an arbitrary struct to pass other test-specifc state
to the callbacks. If its contents vary from trial to trial and it
influences the property test, it should be considered another input and
hashed accordingly.


### alloc - allocate an instance from a random bit stream

```c
    enum theft_alloc_res {
        THEFT_ALLOC_OK,
        THEFT_ALLOC_SKIP,
        THEFT_ALLOC_ERROR,
    };
    typedef enum theft_alloc_res
    theft_alloc_cb(struct theft *t, void *env, void **instance);
```

This is the only required callback.

Construct an argument instance, based off of the random bit stream.
To request random bits, use `theft_random_bits(t, bit_count)` or
`theft_random_bits_bulk(t, bit_count, buffer)`. The bitstream is
produced from a known seed, so it can be constructed again if
necessary. These streams of random bits are not expected to be
consistent between versions of the library.

To choose a random unsigned int, use `theft_random_choice(t, LIMIT)`,
which will return approximately evenly distributed `uint64_t`
values less than LIMIT. For example, `theft_random_choice(t, 5)` will
return values from `[0, 1, 2, 3, 4]`.

- On success, write the instance into `(*instance*)` and return
  `THEFT_ALLOC_OK`.

- If the current bit stream should be skipped, return
  `THEFT_ALLOC_SKIP`.

- To halt the entire test run with an error, return `THEFT_ALLOC_ERROR`.

If **autoshrinking** is used, there is an additional constraint: smaller
random bit values should lead to simpler instances. In particular, a
bitstream of all `0` bits should produce a minimal value for the type.
For more details, see [shrinking.md](shrinking.md).


### free - free an instance and any associated resources

```c
    typedef void
    theft_free_cb(void *instance, void *env);
```

Free the memory and other resources associated with the instance. If not
provided, theft will just leak resources. If only a single
`free(instance)` is needed, use `theft_generic_free_cb`.


### hash - get a hash for an instance

```c
    typedef theft_hash
    theft_hash_cb(const void *instance, void *env);
```

Using the included `theft_hash_*` functionality, produce a hash value
based on a given instance. This will usually consist of
`theft_hash_init(&h)`, then calling `theft_hash_sink(&h, &field,
sizeof(field))` on the instance's contents, and then returning
the result from `theft_hash_done(&h)`.

If provided, theft will use these hashes to avoid testing combinations
of arguments that have already been tried. Note that if the contents of
`env` impacts how instances are constructed / simplified, it should also
be fed into the hash.


### shrink - produce a simpler copy of an instance

```c
    enum theft_shrink_res {
        THEFT_SHRINK_OK,
        THEFT_SHRINK_DEAD_END,
        THEFT_SHRINK_NO_MORE_TACTICS,
        THEFT_SHRINK_ERROR,
    };
    typedef enum theft_shrink_res
    theft_shrink_cb(struct theft *t, const void *instance,
        uint32_t tactic, void *env, void **output);
```

For a given instance, producer a simpler copy, using the numerical value
in TACTIC to choose between multiple options. If not provided, theft
will just report the initially generated counter-example arguments
as-is. This is equivalent to a shrink callback that always returns
`THEFT_NO_MORE_TACTICS`.

If a simpler instance can be produced, write it into `(*output)` and
return `THEFT_SHRINK_OK`. If the current tactic is unusable, return
`THEFT_SHRINK_DEAD_END`, and if all known tactics have been tried,
return `THEFT_SHRINK_NO_MORE_TACTICS`.

If shrinking succeeds, theft will reset the tactic counter back to
0, so tactics that simplify by larger steps should be tried first,
and then later tactics can get them unstuck.

For more information about shrinking, recommendations for writing custom
shrinkers, using autoshrinking, and so on, see
[shrinking.md](shrinking.md).


### print - print an instance to the output stream

```c
    typedef void
    theft_print_cb(FILE *f, const void *instance, void *env);
```

Print the instance to a given file stream, behaving like:

```c
    fprintf(f, "%s", instance_to_string(instance, env));
```

If not provided, theft will just print the random number seeds that led
to discovering counter-examples.


## Hooks

`theft_run_config` has several **hook** fields, which can be used to
control theft's behavior:

```c
    struct {
        theft_hook_run_pre_cb *run_pre;
        theft_hook_run_post_cb *run_post;
        theft_hook_gen_args_pre_cb *gen_args_pre;
        theft_hook_trial_pre_cb *trial_pre;
        theft_hook_fork_post_cb *fork_post;
        theft_hook_trial_post_cb *trial_post;
        theft_hook_counterexample_cb *counterexample;
        theft_hook_shrink_pre_cb *shrink_pre;
        theft_hook_shrink_post_cb *shrink_post;
        theft_hook_shrink_trial_post_cb *shrink_trial_post;
        /* Environment pointer. This is completely opaque to theft
         * itself, but will be passed to all callbacks. */
        void *env;
    } hooks;
```

Each one of these is called with a callback-specific `info` struct (with
progress info such as the currently generated argument instances, the
result of the trial that just ran, etc.) and the `.hooks.env` field,
and returns an enum that indicates whether theft should continue,
halt everything with an error, or other callback-specific actions.

To get the `.hooks.env` pointer in the property function or `type_info`
callbacks, use `theft_hook_get_env(t)`: This environment can be used to
pass in a logging level for the trial, save extra details to print in a
hook later, pass in a size limit for the generated instance, etc.

Note that the environment shouldn't be changed within a run in a way
that affects trial passes/fails -- for example, changing the iteration
count as a property is re-run for shrinking will distort how changing
the input affects the property, making shrinking less effective.

For all of the details, see their type definitions in:
[inc/theft_types.h][1].

[1]: https://github.com/silentbicycle/theft/blob/master/inc/theft_types.h

By default:

- `run_pre` calls `theft_hook_run_pre_print_info` (which calls
  `theft_print_run_pre_info` and then returns
  `THEFT_HOOK_RUN_PRE_CONTINUE`).

- `run_post` calls `theft_hook_run_post_print_info` (which calls
  `theft_print_run_post_info` and then returns
  `THEFT_HOOK_RUN_POST_CONTINUE`).

- `trial_post` calls `theft_hook_trial_post_print_result` (which calls
  `theft_print_trial_result` with an internally allocated
  `theft_print_trial_result_env` struct, and then returns
  `THEFT_HOOK_TRIAL_POST_CONTINUE`).

- `counterexample` calls `theft_print_counterexample` and
  returns `THEFT_HOOK_COUNTEREXAMPLE_CONTINUE`.

All other callbacks default to just returning their `*_CONTINUE` result.

These hooks can be overridden to add test-specific behavior. For example:

- Reporting can be customized (by changing any of several callbacks)

- Halting after a certain number of failures (`gen_args_pre` or
  `trial_pre`)

- Halting after a certain amount of time spent running tests
(`gen_args_pre` or `trial_pre`)

- Running a failing trial again with a debugger attached or logging
  changes (`trial_post`)

- Halting shrinking after a certain amount of time (`shrink_pre`)

- Dropping privileges with `setrlimit`, `pledge`, etc. on the
  forked child process before running the property (`fork_post`).


### Example Output

Here is what example output for a property test might looks like
with the default hooks, indicating what output comes from which hook:

`run_pre` (a banner when starting the run):

    == PROP 'property name': 100 trials, seed 0xa4894b4f1ec336b1

(`gen_args_pre` and then `trial_pre` hooks would be called before each trial)

`trial_post` (printing dots for progress, and a 'd' for a duplicate trial):

    ....d

(`shrink_pre`, `shrink_post`, and `shrink_trial_post` hooks would be
called at this point)

`counterexample` (a failure was found -- printing the arguments):

     -- Counter-Example: property name
        Trial 5, Seed 0xa4894b4f1ec336b1
        Argument 0:
    [0 9 0 ]
    -- autoshrink [generation: 17, requests: 5 -- 24/24 bits consumed]
    raw:  01 48 42

    requests: (5)
    0 -- 3 bits: 1 (0x1)
    1 -- 8 bits: 0 (0x0)
    2 -- 3 bits: 1 (0x1)
    3 -- 8 bits: 9 (0x9)
    4 -- 2 bits: 1 (0x1)

`trial_post` (printing an 'F' for the failure, and moving on):

    F..................

...

`run_post`:

    == FAIL 'property name': pass 26, fail 5, skip 0, dup 1
