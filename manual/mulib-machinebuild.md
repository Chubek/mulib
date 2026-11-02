# mulib-machinebuild — DSL to JavaScript automata

`mulib-machinebuild.h` parses a small declarative language into an owned
[FST](mulib-fst.md) and generates an ES2015 JavaScript factory expression.
It describes automata, **not** native instruction-selection machines from
`mulib-machine.h`. No JavaScript engine is embedded in this module.

## Integration and example

Define all transitive implementation switches in one C11 translation unit:

```c
#define MULIB_IMPL_VEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_FST
#define MULIB_IMPL_MACHINEBUILD
#include "mulib-machinebuild.h"
```

The [standalone example](../examples/modules/machinebuild.c) emits JavaScript:

```sh
cc -std=c11 -Iinclude examples/modules/machinebuild.c -o machinebuild
./machinebuild > machine.js
```

## Supported grammar

```text
machine     = "machine" identifier "{" statement* "}" EOF
statement   = "state" identifier ["accept"] ";"
            | "start" identifier ";"
            | "transition" identifier "->" identifier "on" number
              [".." number] ["emit" number] ";"
identifier  = [A-Za-z_][A-Za-z_0-9]*       (at most 63 bytes)
number      = [0-9]+                      (0 through 4294967295)
```

Whitespace is ASCII space, tab, CR or LF. `#` and `//` introduce line comments.
Keywords are case-sensitive; identifiers in name positions may themselves be
keyword spellings. Numbers are decimal, including leading zeros; negative,
hexadecimal and floating-point literals are rejected. The parser takes an
explicit byte length and does not require a trailing NUL. A terminating NUL
is not part of the grammar. Comments may contain arbitrary non-LF bytes.

Example:

```text
machine example {
  start idle;                         # forward references are allowed
  state idle accept;
  state reading;
  state done accept;
  transition idle -> reading on 97..122 emit 65;
  transition reading -> reading on 0..9;
  transition reading -> done on 4294967295 emit 0;
}
```

Each transition consumes one token. `emit` produces **one constant**, not a
range-relative mapping. Without `emit`, the edge consumes silently. An omitted
range endpoint denotes a singleton. Exactly one start declaration is required;
every referenced state must be declared exactly once. Declarations and
transitions may appear in any order. Duplicate starts/states and overlapping
outgoing ranges are errors. Nonaccepting or unreachable states are legal.
There are no implicit sink transitions, epsilon edges, action expressions,
strings, embedded JavaScript, nondeterminism, or nested/hierarchical states.

## C ownership and diagnostics

`mulib_machinebuild_parse(source, length, limits, &machine, &diagnostic)` returns
an owned machine independent of source lifetime. `limits` and `diagnostic` may
be NULL. Failure leaves the output pointer unchanged and frees all temporary
allocations. Success does not free an old output pointer: destroy any previous
machine explicitly. Release using `mulib_machinebuild_destroy()`; NULL allowed.

Diagnostic fields are status, zero-based byte offset, 1-based line and byte
column, and a borrowed static message. CR counts as a column; LF starts a new
line. Semantic diagnostics point at the offending statement, syntax errors at
the unexpected token, and a missing start at EOF. On success the diagnostic
points at EOF with status OK and message `success`.

Statuses: OK (0), INVALID (-1), NOMEM (-2), LIMIT (-3), SYNTAX (-4), REFERENCE
(-5), CONFLICT (-6). Reversed ranges are SYNTAX; unknown states or a missing
start are REFERENCE. NULL source/output is INVALID (use a non-NULL empty source
to test empty-input syntax).

Zero limit fields select defaults: 1048576 source bytes, 4096 states and
16384 transitions. Limits bound storage/cardinality, not exact parser work.
The two-pass parser supports forward references without recursion, checks
state names linearly and checks each transition against existing outgoing
ranges; worst-case construction time is quadratic in declared records.
All allocation sizes are overflow-checked by the storage modules.

`mulib_machinebuild_fst()` borrows the immutable graph, `start()` returns its
start ID, and `name()`/`state_name()` borrow names. State IDs follow declaration
order. Borrowed data remains valid until destruction. NULL machines return
NULL or `MULIB_FST_NONE`; unknown state IDs return NULL.

`mulib_machinebuild_emit_js(machine, &strbuf)` appends to an initialized MuLIB
string builder, preserving its original content on failure. The caller owns
and frees the builder. Generation has no hidden filesystem writes or external
dependencies. It returns INVALID for NULL parameters and NOMEM for append
failure. Builder inputs must satisfy the strbuf initialization contract.

## Generated JavaScript API

The output is an **expression**, not a named global or a module export. Embed
it directly as source, for example:

```js
const createMachine = /* paste generated expression here */;
const machine = createMachine();
machine.step(97);       // {matched:true, output:[65], state:"reading", accepting:false}
machine.step(3);        // {matched:true, output:[], state:"reading", accepting:false}
machine.step(4294967295); // emits [0], enters "done"
machine.reset();
machine.run([97, 3, 4294967295]); // {accepted:true, output:[65,0], state:"done"}
```

Every factory call has independent incremental state. Read-only getters
`state` (name) and `accepting` describe it. `reset()` restores the start.
`step(token)` returns the shown object. A missing transition returns
`matched:false`, an empty output and unchanged state; acceptance is a separate
property from matching one transition.

`run(tokens)` always starts at the start state, requires full-input acceptance,
and **never changes incremental state**. It returns `{accepted, output, state}`.
Rejected runs discard all emitted symbols; their `state` is the last reached
state, whether rejection was a missing edge or a nonaccepting final state.
It accepts normal arrays and numeric typed arrays (not DataView). Empty input
is accepted iff the start is accepting. Every input token must be an integer
in the uint32 range; invalid values throw RangeError. Invalid containers throw
TypeError. `run` validates the whole array before matching, even tokens after
a would-be missing transition. JavaScript numbers represent all uint32 values
exactly; no signed bitwise coercion is used.

Generated tables are private, and only validated ASCII identifiers and decimal
numbers enter source. The generated runtime has no eval, imports, global state
or dependencies. C and JS agree on transitions/emissions/acceptance, but the
JS runtime has no work-budget API and uses ordinary JS memory/error handling.

## Verification

`tests/test_fst_machinebuild.c` covers parsing, forward references, exact-length
buffers, diagnostics, truncation and deterministic mutations, invalid
syntax/references/ranges, limits, deterministic emission and injected allocation
failures. CMake also runs
`tests/test_machinebuild.js` when Node is available, executing generated code
and checking independent instances, reset/step/run, numeric boundaries,
rejections and runtime argument validation. It also compares JavaScript results
against C transductions for all 2801 sequences of length zero through four over
a seven-symbol boundary-focused alphabet.
