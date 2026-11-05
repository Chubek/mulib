# YANTLR standard library, version 1

An **opt-in provider registry** for [`mulib-yantlr.h`](../../include/mulib-yantlr.h).
It uses only public MuLIB APIs, adds no third-party dependencies, and leaves the
core's explicit linking, immutable values and transactional artifact rules intact.
It does not implement the proposed compiler services in `Lua.y1` or `YBASIC.y1`.
Those specifications still fail with unresolved services, not dummy successes.

## Integration

Add `include` and `library/yantlr` to your include paths (or link the CMake
interface target `mulib_yantlr_stdlib`). In **one** translation unit:

```c
#define MULIB_IMPL_ARENA
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PEG
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_YANTLR
#define MULIB_IMPL_YANTLR_STDLIB
#include "mulib-yantlr-stdlib.h"
```

Create a registry, pass its bindings to the core, and destroy it **after its runs**:

```c
mulib_yantlr_stdlib_t *library = mulib_yantlr_stdlib_create(NULL);
size_t count;
const mulib_yantlr_binding_t *bindings =
    mulib_yantlr_stdlib_bindings(library, &count);
/* Check library and module before evaluating. */
mulib_yantlr_run_t *run =
    mulib_yantlr_evaluate(module, bindings, count, NULL, &error);
/* Inspect values/artifacts, then: */
mulib_yantlr_run_destroy(run);
mulib_yantlr_module_destroy(module);
mulib_yantlr_stdlib_destroy(library);
```

Other translation units include the header without implementation switches.
Implementation translation units are C11; C++ clients can include declarations
and link the C implementation. `MULIB_YANTLR_STDLIB_VERSION` is 1.
Copy the binding array to merge with application providers, or select a subset;
names must remain unique and must not collide with declarations. Bindings borrow
immutable registry state, so retained functions require the registry to remain
alive. Independent runs may share one registry; individual runs are not thread-safe.

Run the complete example from the repository root:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude library/yantlr/example.c -lm -o /tmp/opencode/yantlr-demo
/tmp/opencode/yantlr-demo library/yantlr/example.y1
```

The driver prints staged artifacts; it never publishes files or runs a compiler.

## Exports

All functions curry, and all arguments evaluate **eagerly** in source order.
Arrays with only identifiers can look like qualifiers; parenthesize array
arguments, e.g. `Services[Array]::Map identity ([a,b])`.

| Export | Arguments → result |
|---|---|
| `identity` | value → same value |
| `Services[Core]::Type` | value → `"null"`, `"bool"`, `"number"`, `"string"`, `"array"`, `"record"`, `"function"`, or `"opaque"` |
| `add`, `sub`, `mul`, `div`, `mod` | number, number → number; `mod` uses C `fmod` (remainder has dividend's sign) |
| `eq`, `neq` | same-type scalars → boolean; numbers, strings, booleans and null only |
| `lt`, `leq`, `gt`, `geq` | number pair or string pair → boolean |
| `not` | boolean → boolean |
| `and`, `or` | boolean, boolean → boolean (**not** short circuit) |
| `Services[Core]::Choose` | boolean, then-value, else-value → selected value (both evaluated) |
| `Services[Array]::Length` | array → count |
| `Services[Array]::At` | array, index → element; absent index is an error |
| `Services[Array]::Range` | count → `[0, ..., count-1]` |
| `Services[Array]::Concat` | array, array → concatenated array |
| `Services[Array]::Map` | function, array → mapped array |
| `Services[Array]::Filter` | predicate, array → selected original elements; predicate must return boolean |
| `Services[Array]::Fold` | function, initial, array → left fold; function receives accumulator then element |
| `Services[String]::Length` | string → UTF-8 **byte** count, including NULs |
| `Services[String]::Concat` | string, string → concatenated string |
| `Services[String]::Slice` | string, start-byte, byte-count → substring; must remain valid UTF-8 |
| `Services[String]::Join` | separator, string-array → joined string; empty array produces `""` |
| `Services[Record]::Keys`, `Services[Record]::Values` | record → array in insertion order |
| `Services[Record]::Get`, `Services[Record]::Has` | record, string-key → value / boolean; Get on absent key is a name error |
| `Services[JSON]::Parse` | string → value; exact RFC 8259 JSON via MuLIB JSON |
| `Services[JSON]::Stringify` | JSON-compatible value → compact JSON string |
| `Services[Diag]::Levels::Debug`, `Info`, `Warning`, `Error` | `Numeric` → 0, 1, 2, 3 respectively (each has the full `Services[Diag]::Levels::` prefix) |
| `Numeric` | constant `"numeric"`, used as `Services[Diag]::Levels::Warning!Numeric` |
| `Services[Diag]::Assert` | boolean, message-string → true, or fail run with message |
| `Services[Diag]::Fail` | message-string → fail run with message |
| `Services[IO]::Emit` | relative-path-string, content-string → content; stages a copied artifact |
| `Services[Parse]::PEG`, `Services[Parse]::EBNF` | grammar-string → run-owned compiled opaque grammar |
| `Services[Parse]::Run` | compiled grammar, input-string → match record |

Numbers are finite binary64, not arbitrary-precision integers. Overflow,
division/remainder by either signed zero, type mismatches and invalid indices
fail the run. Indices/counts are nonnegative exact integers, at most 2^53−1 and
strictly less than `SIZE_MAX`. String ordering compares unsigned UTF-8 bytes;
there is no locale collation, normalization or implicit coercion. Record keys
and string operations preserve embedded NULs. Diagnostic messages and artifact
paths reject NULs rather than truncating. Failures preserve the first core error;
assertion/fail messages are copied into its 256-byte diagnostic buffer.

Use closures for conditional work: `Choose condition (\x -> ...) (\x -> ...)`
selects an unevaluated body; then apply the chosen closure to an argument.
Map/filter/fold invoke callbacks left-to-right, stopping at the first failure.
Empty map/filter/concat/keys/values are valid; empty fold returns its initial value.
JSON rejects functions and opaque values, including nested ones. Serialization
preserves record order and NUL keys; it is not a canonical hashing format.

### Parsing

PEG and EBNF use the exact dialects in [`mulib-peg.md`](../../manual/mulib-peg.md),
not the prose mini-languages from the design specimens. The first rule is the
entry, and `Run` always requires the **entire** input. Grammar actions are
metadata, never evaluated code. No lexer, whitespace skipping or recovery is
installed implicitly. Grammars reject left recursion and nullable repetition.

```text
set Grammar Services[Parse]::PEG!"document <- word (' ' word)*; word <- [a-z]+;";
set Match Services[Parse]::Run Grammar "hello world";
```

The result is `{ "matched": bool, "consumed": number, "offset": number,
"captures": [...] }`. Mismatch is a normal result (`matched=false`), not a
poisoned run. `offset` is the parser's farthest error byte on mismatch, or the
consumed byte count on success. Captures are postorder records with `rule`
(name), `offset`, `length` and `first`; `[first, capture-index)` contains that
capture's descendants. They refer to the input's byte spans, not copied lexemes.
Grammar syntax, allocation and parser resource failures do poison the run.
Opaque grammar ownership transfers only after successful construction, and it
is released with the run, including on later evaluation failure.

### Bounds and side effects

`mulib_yantlr_stdlib_options_t` zero defaults are 100,000 items, 16 MiB of text
and depth 64 **per operation**. Depth is clamped to 128 for native-stack safety.
`max_items` bounds array traversal/results, record lookup traversal and JSON
node traversal; `max_bytes` bounds input/output strings and aggregate copied
record-key/JSON text bytes. JSON serialization checks encoded size after
serialization (escaping can temporarily expand the text by roughly sixfold,
plus node punctuation). JSON parsing checks node/depth limits after the bounded
input has been parsed; MuLIB JSON's own parser depth limit also applies.
These are not strict allocator quotas. Constant-time array length/index queries
do not traverse the array and are not subject to `max_items`; operations returning
existing values do not recheck their text size. MuLIB JSON's combined numeric
range/allocation diagnostic maps to a syntax error (as in the core host parser);
unambiguous allocation failures map to memory errors.

`parser` supplies parsec depth/step/capture limits for `Run`; captures are also
capped by `max_items`. Grammar compilation has MuLIB PEG's own node/depth limits.
Core evaluation fuel, value count, depth and artifact byte limits remain active,
including during map/filter/fold callbacks. Each call gets a fresh provider
operation budget, not a global work quota; use the core's limits as well.

Emit only stages UTF-8 string bytes, using the core's canonical path validation,
duplicate detection and aggregate artifact budget. The caller publishes artifacts
after successful evaluation. There is no filesystem read service, ambient import
search, process execution, network access, diagnostic sink, compiler toolchain,
or registration of unimplemented `Gen[Language]`/semantic/IR services.

Tests: `tests/test_yantlr_stdlib.c` covers real registry evaluation, the shipped
`.y1` example, currying/closures, binary-safe text, parsing, failures and limits;
`tests/test_yantlr_stdlib_alloc.c` sweeps allocation failures and checks cleanup.
