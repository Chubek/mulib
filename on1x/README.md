# On1x Standard Library + native PPX extension batch

These extensions are integrated with the existing C11 compiler and module loader.
The refined Basis modules, `stdlib/ppx/`, `stdlib/sexpr/`, `stdlib/visitor/`,
original native processors and contracts are preserved.

## Project tools

Use `on1xc init --bin`, `on1xc init --proj`, or `on1xc init --lib` to
scaffold an executable, a multi-target project, or a source library. Build
from the project root with `on1xc exec build` or `on1xc exec release`. The
[On1xfile guide](stdlib/on1xfile/README.md) describes root policy,
directory-local input declarations, Mune profiles, and dependencies. The
`--sandbox` build option requires Linux and bubblewrap and does not fall back
to a host build.

Builds do not fetch dependencies. Use the separate
[`on1x-project.py lock`/`fetch` commands](stdlib/on1xfile/README.md#locking-and-explicit-remote-git-acquisition)
to resolve branch/tag intent to immutable commits and acquire a Git library
over HTTPS. `On1xfile.lock` plus `build --frozen` keeps later builds offline.

The [source formatter](docs/formatter.md) runs as `on1xc fmt`, reads
`.on1xfmt`, and can also emit LaTeX or troff listings. Use `--inplace` (`-i`)
to rewrite the input as formatted source. Formatting itself does not run
project scripts or PPX.

See [editor and knowledge addons](addons/README.md) for Vim/Neovim, VS Code,
Sublime Text, `bat`, the optional On1x language server, and the
[read-only ChatGPT knowledge MCP](addons/chatgpt/README.md). The ChatGPT addon
retrieves cited repository evidence; it does not compile code or admit safe
bindings, and it requires a separately hosted HTTPS endpoint or Secure MCP
Tunnel.

For C/C++ build-tool integrations, see the [On1x extension SDK](sdk/README.md).
SDK hook commands can discover isolated external plugins through
`ON1X_PLUGIN_PATH` or select them with `--plugin +NAME_OR_PATH`. Ordinary
compilation does not load SDK plugins.

## Language implementation continuation

The [core literal forms](docs/core-literals.md) add UTF-8 `char`, `b!` byte/buffer literals, raw and
triple-quoted strings, interpolation, and binary/octal integers. The
[enum connective](docs/core-enum-connective.md) adds `value ~ { … }`, and
[call-scoped references](docs/core-references.md) allow `ref i64` parameters
to update a named caller binding with `:=`. [Core regexes](docs/core-regex.md)
use Firestone at compile time and support UTF-8 matching and a bounded capture
profile. [Nominal ADTs](docs/owning-adts.md) can share ownership of immutable
`sexpr` fields. That shared-owner profile remains nonrecursive; the finite
unique-owner profile below admits recursion only through `own<Named>`, while
general resource fields still reject.
None of these constructs require PPX.

The [cancellation/deadline continuation](docs/cancellation-deadlines.md) now
executes consuming cancellation of running or blocked internal fibers, explicit
defer/lexical cleanup, descendant draining, and monotonic `poll after` choices
through C11. The same profile now admits `taskgroup within 1s { ... }`:
expiry drains cooperative cleanup and terminates the run, not a typed timeout
result (private CSP ABI v4). See
[`examples/cancellation_deadlines.on1x`](examples/cancellation_deadlines.on1x).
**Async/await is not implemented**: verified stackless suspension/frame
lowering is still required. This is not completion of M4.

An initial [cooperative fiber/channel profile](docs/concurrency.md) now executes
`taskgroup`, `launch { ... }`/`join`, 1:1 `i64` unidirectional bounded and
rendezvous channels, and bounded atomic `poll` through generated C11 and existing MuLIB kernels. The real
source [channel example](examples/cooperative_channels.on1x) covers checked send/receive,
close and scoped cleanup; the [poll example](examples/cooperative_poll.on1x)
selects a rendezvous receive. This is **not** completion of the core-syntax
specification: typed timeout recovery, the full portable shell grammar, async/await, duplex,
general payloads and native worker threads remain unsupported. See the
[verification record](VERIFICATION.md) for commands, current suite results and
the intentional optional-profile skip.

A separate bounded [external-process profile](docs/concurrency.md) admits
`launch exec("/absolute/program", "literal-arg")`, compiler-parsed single/triple
backtick command literals, consuming `join`, and process-group `cancel` through
[compiled C11](examples/cooperative_external.on1x). `launch capture` returns an
affine process/stdin/stdout/stderr tuple whose streams are bounded byte-channel
endpoints; the [stream example](examples/cooperative_process_streams.on1x) writes
stdin and drains stdout. No host shell is invoked: expansion, assignments,
pipelines, redirection and command lists reject. A newly launched
internal fiber can be cancelled before dispatch or cooperatively while running/
blocked; registered cleanups run before consuming `cancel handle` returns.

Atomic [`poll` now selects captured byte streams](docs/stream-poll.md), including
mixed i64/u8 choices, exact receive binder types, send backpressure and distinct
EOF arms. The [compiled example](examples/cooperative_stream_poll.on1x) combines
process stdout and a normal channel. The later continuation adds `poll after`
deadlines, not async/await or general byte-channel construction.

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
argument identity and C11 execution (`examples/nominal_adts.on1x`). Nominal
values may retain [immutable shared `sexpr`](docs/owning-adts.md), and the
[finite unique-owner profile](docs/recursive-owning-adts.md) adds
`own<Named>`, `box`, explicit `move`, consuming `take`/`drop`, and recursive
layouts through strict C11. Unique ownership remains closed to MIR, PPX, raw
FFI, captures, staging, defer, and concurrency; there is no intrinsic
list/iterator protocol or general borrow system.

The final type-checker hardening matrix covers ownership together with inherent
`impl` and cliche admission, including forged HIR and generated-member metadata.
Current final results: generated GCC **98/98**, fallback GCC **97/97**, and
focused Clang ASan/UBSan **15/15**. See
[`VERIFICATION.md`](VERIFICATION.md) for exact commands and remaining limits.

Typed [multi-stage programming](docs/staging.md) accepts both expression quotes
and lexical quotation regions such as
`quote { let sampled = lift(input); sampled + 2 }`. Region statements, returns,
locals and cleanup execute through the existing AOT C11 thunk path; the feature
does not add JIT, source evaluation, or a new PPX/runtime ABI.
The current focused evidence passes **12/12** generated-scanner GCC,
**10/10** fallback-scanner GCC, and **10/10** fallback Clang ASan/UBSan tests.
The complete inventories retain two unrelated baseline MuLIB inventory
failures; exact commands and results are in
[`VERIFICATION.md`](VERIFICATION.md#restricted-multistage-lexical-quotation-regions--2026-11-19).

The M2 [defer/typed-error profile](docs/defer-errors.md) completes Go-style
`defer call;` and postfix nominal `Result<T,E>?` through C11. Calls snapshot their
callee/arguments at registration and run LIFO at **function return**, including
propagated errors; loop exits do not drain them. Registration has finite count/
capture-byte budgets. Use `match` for recovery; fatal traps still do not unwind.
See [`examples/defer_errors.on1x`](examples/defer_errors.on1x). General exceptions,
resource-bearing results, async cancellation and MIR/SDK defer models remain unsupported.

The default [pervasive environment](docs/modules.md#the-pervasive-environment)
now exposes canonical bounded `Option<T>`/`Result<T,E>` types and focused
`option_*`/`result_*` helpers without imports. These are ordinary generic On1x
declarations, preserve explicit-import/local-shadow precedence, and add no ABI.
Payloads are still limited to trivially copied nominal data; legacy tuple,
scalar-pair and S-expression facades remain explicit compatibility APIs. See
[`examples/pervasive_options.on1x`](examples/pervasive_options.on1x).

Intrinsic [core cliches](docs/core-cliche.md) provide coherent constrained
generics, generic members, associated values, exact-over-blanket selection, and
compile-time qualified lookup such as `Show<T>::show(value)`. Executable
projections lower to deterministic ordinary functions; there is no runtime
implementation-name lookup. The orphan/overlap/decreasing-prerequisite rules and
finite resolution budgets fail closed. This is separate from inherent
`Type::member` lookup and from the legacy Category processor below.

Intrinsic [nominal `impl` blocks](docs/core-impl.md) now provide associated
functions/values and copy-safe by-value receiver members for records and enums,
including generic targets and inferred polymorphic members. `value::member`
lowers the receiver to the first ordinary argument, while `Type::member` performs
associated lookup; dispatch is deterministic and has no runtime registry.
`ref self`, `move self`, and managed/resource value receivers remain explicitly
unsupported; owning values use ordinary or non-receiver functions with explicit
consuming parameters. Unit-v1 PPX/interface transport and aggregate MIR also
remain unsupported.

Category now has an executable **M0/M1 plus concrete M2 slice**: `@enable Protocols`
invokes a bundled native On1x-authored processor for `@extension Protocols { ... }`
declarations and generates explicit, normally typechecked witness selectors.
Native post-inference planning and independent host admission check member types,
module provenance and exact selector forwarding. `--emit-category-plan` reports
the checked choices and source/artifact fingerprints.
[Category](docs/category.md) documents the version-2 processor, activation and installation.
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

[KernelBind](docs/kernel-bind.md) recognizes `import kernel` in the compiler and
supports explicit ExoLib/ExoML discovery of selected canonical MuLIB profiles.
The [sandbox profiles](docs/kernel-sandbox.md) expose build and declaration
evidence only, not callable or safe imports.
The opt-in C11 build profile can call all three `mulib.clamp` functions via
`unsafe import kernel`. The [review-gated scalar build](docs/kernel-review.md)
also accepts safe clamp and [bounded numerical imports](docs/kernel-numerics.md)
with an independently signed, operator-selected review. Tests use an ephemeral
authority; no production approval is shipped. Owned foreign resources remain
closed.

The repository accessed through the GitHub connection exposed the prior PPX-v1 schema and Exotract/ExoML contract catalogs. The `on1x/std/ppx` path in the request appears to refer to the actual `on1x/stdlib/ppx` path, whose `ppx.on1x` is used by all newly added native PPXs.

## Contents

- Native [`GrammarEmbed`](stdppx/grammarembed/README.md): Perl 6-like grammar
  declarations using Firestone liblr to emit On1x NFA/DFA lexers and
  LR(1)/LALR(1)/LL(1) parsers, including spans and concrete parse trees.

- Native [`LuaEmbed`](stdppx/luaembed/README.md) and its embedded `llembed` Lua
  module: bounded Mune execution, captured program-stream output, schema-checked
  AST/program generation and explicit metaprogram chains; no runtime Lua dependency.

- Source-only `.on1x` facades: JSON, YAML, TOML, INI, Parsec, crypto, Unix, sockets, network, IPC, NNG-style envelopes, RPC JSON request construction, Shexp ASTs, hashconsing, graph traversal, Base and Batteries-like structures.
- `_bridge/mulib_bridge.c`: raw C ABI bridge against *real MuLIB public headers* and an independently built Exotract one-TU `libmulib.a`. C bridge is **not** safe by ExoML/Chisel criteria. It requires reviewed extraction and independent memory/effect checks before a safe FFI claim.
- Seventeen ordinary native On1x PPXs: the original eight plus assertions, cond,
  format, logging, state_machine, tap, trace and native-only LuaEmbed/GrammarEmbed, with
  compiler-generated C11 execution tests.
- [`stdlib/CATALOG.json`](stdlib/CATALOG.json): complete named SML Basis inventory;
  extension modules remain partial adapters, not Base/Batteries API parity.

## Library, PPX and kernel discovery

`stdlib/mulib` is part of the installed standard-library tree (`std.mulib`).
Colon-separated `ON1X_LIB_PATH`, `ON1X_STDLIB_PATH`, `ON1X_PPX_PATH`,
`ON1X_STDPPX_PATH` and `ON1X_MULIB_PATH` have matching CLI flags:
`--lib-path`, `--stdlib-path`, `--ppx-path`, `--stdppx-path`, `--mulib-path`
(kernel paths are build-tool options). Imports and `--use-ppx NAME` no longer
need individual paths when installed in these directories. Standard-library
and native PPX directories advertise versioned `On1xfile` manifests; discovered
script PPXs carry terse metadata on their second line, after the shebang.
PPX activation remains explicit. See [search order, manifest examples and
compatibility](docs/search-paths.md).

## Build (C11 adapter)

The build helper selects bridge/kernel groups automatically and preserves one
kernel implementation TU. Alternatively supply a locked Exotract archive through
`--mulib-archive DIRECTORY`; do not define kernel switches in consumer adapters.
Building `on1xc` requires Python 3 for the PrettyPrint kernel plan. The compiler
does not invoke Python to format source.

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

The bounded core diagnostic registry uses a build-validated JSON v2 catalog for
new diagnostic identity, presentation and policy, with runtime replacement,
file-local `@scrutiny`, CLI/On1xfile policy, and message-free registered PPX
advisories. Existing compiler messages remain a documented legacy boundary.
When available,
Wetstone analyzes a conservative straight-line checked-scalar subset; Syren adds
JSON/SARIF presentation and `on1xc --repair INPUT -o NEW_FILE`. Strict scrutiny
escalates supported findings but does not turn skipped HIR into a proof. Wetstone
proposals are advisory and unvalidated; only independently rechecked Syren edits
are labeled compiler-valid, never intent-equivalent or in-place changes.
See [diagnostics, scrutiny, and repair](docs/diagnostics.md).
