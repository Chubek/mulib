# On1x Standard Library + native PPX extension batch

These extensions are integrated with the existing C11 compiler and module loader.
The refined Basis modules, `stdlib/ppx/`, `stdlib/sexpr/`, `stdlib/visitor/`,
original native processors and contracts are preserved.

## Language implementation continuation

An initial [cooperative fiber/channel profile](docs/concurrency.md) now executes
`taskgroup`, `launch { ... }`/`join`, 1:1 `i64` unidirectional bounded and
rendezvous channels, and bounded atomic `poll` through generated C11 and existing MuLIB kernels. The real
source [channel example](examples/cooperative_channels.on1x) covers checked send/receive,
close and scoped cleanup; the [poll example](examples/cooperative_poll.on1x)
selects a rendezvous receive. This is **not** completion of the core-syntax
specification: `poll after` deadlines, running/blocked internal-fiber cancellation, portable
shell literals/streams, async/await, duplex,
general payloads and native worker threads remain unsupported. See the
[verification record](VERIFICATION.md) for commands and current suite failures.

A separate bounded [direct-exec external-fiber profile](docs/concurrency.md)
admits `launch exec("/absolute/program", "literal-arg")`, consuming `join`,
and process-group `cancel` through [compiled C11](examples/cooperative_external.on1x).
It does not interpret shell text or capture typed streams. A newly launched
internal fiber may also be cancelled by an immediately following standalone
`cancel handle;` before it runs; blocked internal fibers cannot be cancelled.

The M2 [immutable C-string FFI slice](docs/ffi-strings.md) extends existing
review-gated admission with exact call-scoped inputs and immutable static
results, directional Chisel evidence and null/byte-limit checks. It does not
certify or relabel the shipped raw MuLIB adapters; independent contract review
and general ownership/effect lowering remain blockers.

Intrinsic [core control and matching](docs/core-syntax.md) implements a G1/G2
slice of `ON1X_CORE_SYNTAX_ADDITIONS.md`: statement loops, checked i64 ranges,
lexical exits, and guarded structural `match ... with` statements/expressions.
Typed decision regions execute through C11; scalar/numeric tuple matches also
execute through existing MIR. Intrinsic [value loops](docs/value-loops.md) now
add `loop` expressions and typed `break [label] with value` joins, including
existing lexical cleanup. The next [nominal-data slice](docs/nominal-adts.md)
adds nonrecursive named records and parameterized enums, with exact module/type
argument identity and C11 execution (`examples/nominal_adts.on1x`). Recursive or
owning ADTs, owning lists and general concurrency remain unsupported; the specification
is not complete.

The M2 [defer/typed-error profile](docs/defer-errors.md) completes Go-style
`defer call;` and postfix nominal `Result<T,E>?` through C11. Calls snapshot their
callee/arguments at registration and run LIFO at **function return**, including
propagated errors; loop exits do not drain them. Registration has finite count/
capture-byte budgets. Use `match` for recovery; fatal traps still do not unwind.
See [`examples/defer_errors.on1x`](examples/defer_errors.on1x). General exceptions,
resource-bearing results, cancellation and MIR/SDK defer models remain unsupported.

Category now has an executable **M0/M1 local scalar slice**: `@enable Protocols`
invokes a bundled native On1x-authored processor for `@extension Protocols { ... }`
declarations and generates explicit, normally typechecked witness selectors.
[Category](docs/category.md) documents native pinning, activation and installation.
Constrained generics, associated types, functors/sealing and auto protocols remain
unsupported; `CATEGORY.md` is still the broader implementation specification.

The M1 numeric value slice adds real `f64` to the existing HIR/C11 compiler:
arithmetic/comparisons, explicit checked conversions, exact bit transport and hex
printing, closures/tuples/staging, and raw C `double` leaves/results. The initial
profile is Linux x86_64 GCC/Clang. Decimal literals and MIR v2 now execute floats
and nested trivial tuples, with checked serialization, an evaluator and C11
emission; legacy scalar MIR remains v1. A further M1 slice adds the bounded value
data model: structural records, closed tagged sums with exhaustive `match`,
fixed arrays and owning snapshot slices — all by-value C11 with bounds-checked
access and terminal located value errors (`examples/data_model.on1x`). One M2
prerequisite now admits reviewed **scalar-only** C imports after exact ABI,
ExoML-review-metadata, Chisel-use and executable target gates; this is not
general ownership/resource-safe FFI. The v2 profile adds exact C `_Bool`/binary64
`double` adapters and typed `bool`/`f64` admission alongside signed integers.
The numeric continuation adds distinct `u8/u16/u32/u64` and IEEE `f32` values,
checked arithmetic, explicit wrapping/bit operations, conversions and exact
scalar FFI. Scalar numeric MIR v3 now verifies, round-trips, evaluates and emits
C11 for these types, including tuples, calls, branches and slots. Nerotypical
checks their HIR type obligations; Chisel additionally models numeric tuples.
Suffixed literals include `255u8`, `18446744073709551615u64` and `1.5f32`;
unsuffixed literals retain i64/f64 semantics. See
[`examples/numeric_literals.on1x`](examples/numeric_literals.on1x) and
[`examples/unsigned_f32.on1x`](examples/unsigned_f32.on1x). Floating execution
is still supported only for the tested, guarded Linux x86_64 GCC/Clang profile.
General ownership, foreign resources and multithreaded/async concurrency are
still not admitted. See the
[numeric contract](docs/numerics.md), [data-model contract](docs/data-model.md),
[capability matrix](docs/capability-matrix.md) and
[verification record](VERIFICATION.md). This does not complete M1 or On1x.

The repository accessed through the GitHub connection exposed the prior PPX-v1 schema and Exotract/ExoML contract catalogs. The `on1x/std/ppx` path in the request appears to refer to the actual `on1x/stdlib/ppx` path, whose `ppx.on1x` is used by all newly added native PPXs.

## Contents

- Source-only `.on1x` facades: JSON, YAML, TOML, INI, Parsec, crypto, Unix, sockets, network, IPC, NNG-style envelopes, RPC JSON request construction, Shexp ASTs, hashconsing, graph traversal, Base and Batteries-like structures.
- `_bridge/mulib_bridge.c`: raw C ABI bridge against *real MuLIB public headers* and an independently built Exotract one-TU `libmulib.a`. C bridge is **not** safe by ExoML/Chisel criteria. It requires reviewed extraction and independent memory/effect checks before a safe FFI claim.
- Fifteen native On1x PPXs: the original eight plus assertions, cond, format,
  logging, state_machine, tap and trace, with Python/native execution comparisons.
- [`stdlib/CATALOG.json`](stdlib/CATALOG.json): complete named SML Basis inventory;
  extension modules remain partial adapters, not Base/Batteries API parity.

## Build (C11 adapter)

The build helper selects bridge/kernel groups automatically and preserves one
kernel implementation TU. Alternatively supply a locked Exotract archive through
`--mulib-archive DIRECTORY`; do not define kernel switches in consumer adapters.

```sh
cmake -S on1x -B build/on1x -DBUILD_TESTING=ON
cmake --build build/on1x --parallel 4
python3 on1x/tools/on1x-build.py on1x/examples/stdlib_extensions.on1x \
  --on1xc build/on1x/on1xc -o build/stdlib-example
build/stdlib-example
ctest --test-dir build/on1x --output-on-failure
```

Tests execute canonical MuLIB JSON/YAML/TOML/INI, cryptohash, numeric-IPv4 sockets,
pure helpers and actual native PPXs. See [VERIFICATION.md](VERIFICATION.md) for
the measured GCC/Clang/sanitizer configurations and original historical reports.

## Status caveats

These are **not** complete Jane Street Base, Batteries or SML Basis ports.
`shexp` does not execute processes, `nng` is not libnng, IPC is an in-process
mailbox helper and LibStone interchange does not wrap C++ APIs. Scalar text is
NUL-free; socket hex reception preserves binary bytes. Tuple-returning wrappers
distinguish missing/error/EOF/empty outcomes, but remain trusted raw adapters.
Record/slice/resource ABI and general ExoML/Chisel safe-import lowering remain
open. `on1x-bind emit-safe` is limited to reviewed signed/unsigned-integer, `_Bool`,
binary32 `float`, binary64 `double` and void-result functions with no ownership, error, callback
or concurrency obligations; a user-declared raw `double` ABI alone is not safe
foreign admission. See [admission profiles and versioning](docs/contracts.md#reviewed-scalar-only-safe-admission).
Read [the bridge manual](stdlib/_bridge/README.md), [PPX manual](stdppx/README.md)
and `AGENTS.md` before production use.
## Error reporting and repair proposals

Optional Syren integration (`-DON1X_WITH_SYREN=ON`) adds text/JSON/SARIF source
diagnostics and `on1xc --repair INPUT -o NEW_FILE`. Repairs are independently
compiler-checked proposals, never in-place edits or changes to ON1X's syntax or
semantics. See [diagnostics and repair](docs/diagnostics.md).
