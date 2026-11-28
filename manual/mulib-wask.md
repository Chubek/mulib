# mulib-wask

[Manuals](README.md) · [Library index](index.md)

A small, embeddable **i32 WebAssembly binary module runtime**. This is a focused
profile with straight-line function bodies and direct internal calls, not a
complete WebAssembly engine or WASI host. Use only with controlled input;
validation and limits are not a sandbox guarantee. No third-party library,
native code generation, filesystem or network access.

Define `MULIB_IMPL_WASK`, `MULIB_IMPL_DSYMSPEC`, `MULIB_IMPL_JSON`,
`MULIB_IMPL_PARSEC`, `MULIB_IMPL_AST`, `MULIB_IMPL_STRBUF`, and `MULIB_IMPL_UTF8`
in exactly one C11 translation unit before including `mulib-wask.h`. No switch
is enabled implicitly. Declarations have C++ linkage guards.

`mulib_wask_load(bytes, length, &error)` validates and copies the binary. Destroy
the returned owned module with `mulib_wask_destroy`; NULL is harmless. Source
bytes may be released immediately. A failed load releases partial allocations.
Call an exported function with `mulib_wask_call(module, name, arguments, count,
fuel, &result, &error)`. Arguments/results are unsigned i32 bit patterns; signed
interpretation is instruction-specific. Result changes only on success; void
functions produce zero. Modules are immutable, so calls may run concurrently;
destroy only after all calls have finished. No host callbacks or shared globals.

The loader reads the builtin `wasm` architecture through `mulib-dsymspec.h`, finds
each supported mnemonic's form and encoder, and takes the first `EMIT8` opcode
from its metadata. It does not execute encoder pseudo-code or use its modeled
fixed-width immediates: the runtime decodes actual bounded u32/s32 LEB128.
Metadata is released after the per-module dispatch map is built.

Supported binary format: magic/version 1; ordered, unique type (1), function
(3), export (7), code (10) sections; arbitrary custom (0) sections with valid
UTF-8 names. Empty modules are valid. Function signatures have i32 parameters
and zero or one i32 result. Locals are zero initialized. Function export names
are unique UTF-8 byte strings; the call API addresses names without embedded
NULs. Custom payloads are skipped after name validation.

Supported instructions: `nop`, terminal `end`, `call`, `drop`, `select`, `local.get`,
`local.set`, `local.tee`, `i32.const`, `i32.add`, `i32.sub`, `i32.mul`,
`i32.div_s`, `i32.and`, `i32.or`, `i32.xor`, `i32.eqz`. Addition, subtraction
and multiplication wrap at 32 bits. Signed division truncates toward zero and
traps for zero divisor or INT32_MIN / -1. Every function is decoded and its
stack/local accesses checked during loading, even if never called.

`call` takes a u32 LEB128 function index into the same module, including
non-exported functions and forward references. The validator checks the index
and the callee signature's effect on the caller's operand stack. The callee
receives arguments in declaration order, with fresh zero-initialized locals,
and appends its result to the caller's stack only if its signature has one.

Other instructions and sections are rejected, including imports, memory,
globals, tables, `call_indirect`, tail calls, structured control flow, `return`,
unreachable (polymorphic dead code), floats, i64, SIMD and WASI. No unsupported
feature is silently ignored. Unsupported features produce `MULIB_WASK_UNSUPPORTED`
when encountered as such; malformed framing and resource limits may take
precedence.

Hard limits: 1 MiB module; 128 types/functions/exports each; 256 parameters plus
locals per function; 256 local declaration groups; 256 operand stack entries
per function. Execution uses at most 16 frames including the exported entry
(about 33 KiB of automatic storage on a typical 64-bit host), with no native
recursion. Recursive cycles can validate, but execution cannot exceed the frame
limit. Each instruction, including `call` and each `end`, consumes one unit of
the caller-provided fuel shared across all frames. Fuel exhaustion or exceeding
the frame cap returns `MULIB_WASK_LIMIT`; zero fuel executes nothing. Other
statuses distinguish invalid bytes/arguments, allocation failure, arithmetic
traps and missing exports. Error messages are static; offsets are decoder positions in the module
(after the offending byte/immediate in many cases), not instruction indices.
An optional error pointer can be NULL.

See `examples/modules/wask.c` and `tests/test_wask.c` for execution, ownership,
wrapping arithmetic, traps, truncation, malformed LEBs and bounded mutations.

`mulib_wask_version()` returns API revision 1.
`mulib_wask_supports(mnemonic)` reports exact instruction support without loading a module.

## Browser additions reconciliation ledger

The Wasm staging names below are present as static-inline headers at the audited
base, but are absent from the module manifest and lack per-module manuals or
examples. Their shared smoke test covers only section directory reads, memory
helper bounds and a division trap. This slice extends the existing registered
runtime; it does not adopt or expose these staging APIs as browser capabilities.

| Staging kernel | Closest existing support and disposition |
| --- | --- |
| `wasm-decode` | Envelope helpers only. Wask has its own section-bounded u32/s32 LEB and instruction decoder; unchanged. |
| `wasm-validate` | Envelope order alone does not validate Wasm. Wask validates its supported section contents and function bodies, now including direct-call signatures. |
| `wasm-types` | Staging byte-size lookup does not imply runtime type support. Wask validates i32 parameters/locals and at most one i32 result. |
| `wasm-module` | Staging directory borrows bytes and indexes envelopes; Wask owns an immutable copy and validates before publishing. |
| `wasm-instance` | Staging attachment is metadata, not instantiation. Wask runs isolated call frames with no separate mutable instance or start function. |
| `wasm-memory` | Staging storage helper is not interpreter linear memory. Memory sections and instructions remain unsupported. |
| `wasm-table` | Staging table helper stores opaque handles. Function tables and indirect calls remain unsupported. |
| `wasm-exec` | Staging arithmetic helpers do not interpret modules. Wask is the existing interpreter and now has an explicit frame stack for same-module calls. |
| `wasm-link` | Staging name/kind lookup does not establish type compatibility or permission. Wask does not link imports or resolve host functions. |
| `wasm-jsbridge` | Staging numeric conversion is not a JS binding. Wask accepts explicit i32 bit patterns and has no browser or JS integration. |

The focused suite checks nonminimal call indices, malformed calls,
caller/callee stack behavior, frame reuse, nested traps and shared fuel. It
validates mutated binaries without executing them. No official Wasm conformance
suite or untrusted runtime admission is claimed.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-wask.h`](../kernels/mulib-wask.h) |
| Implementation switch | `MULIB_IMPL_WASK` |
| Repository-local includes | [mulib-dsymspec](mulib-dsymspec.md), [mulib-utf8](mulib-utf8.md) |
| Complete example | [`examples/modules/wask.c`](../examples/modules/wask.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/wask.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
