# Function-scoped defer and typed error propagation (M2)

This semantics increment follows the user's Go-style defer requirement. It
**supersedes** the lexical-scope defer proposal in core-additions section 4.
Previously no intrinsic defer was admitted, so existing programs do not gain
implicit cleanup behavior.

```on1x
enum Result<T,E> { Ok(T), Err(E) }
fn read(n:i64)->Result<i64,u8> {
  defer println(n);
  if n < 0 { return variant<Result<i64,u8>>(Err, 7u8); }
  return variant<Result<i64,u8>>(Ok, n);
}
fn twice(n:i64)->Result<i64,u8> {
  defer println(99);
  let value = read(n)?;
  return variant<Result<i64,u8>>(Ok, value * 2);
}
```

The default pervasive environment now provides a canonical declaration with
this exact protocol plus `result_ok`/`result_error` and combinators. The explicit
declaration above remains valid and intentionally has its own nominal identity;
local declarations take precedence over the automatic prelude.

## Defer contract

The later [CSP continuation](cancellation-deadlines.md) also runs registered
function defers on cooperative internal-fiber cancellation. These explicit
cleanup returns are shielded; fatal traps remain non-unwinding, and there is
still no general exception/async/effectful-MIR model.

- `defer f(args);` requires a call expression. The callable is evaluated first,
  followed by the arguments once, left-to-right, **when registered**. Invocation
  is delayed. Copyable values are snapshotted; RC values retain independent
  references. Shared buffers retain their object, not a deep copy of contents.
- Each dynamic registration belongs to the current function invocation (or
  lambda invocation), not the nearest block or loop. Reached registrations run
  exactly once, in reverse order, on explicit/implicit return and propagated
  `Err`. Loops accumulate registrations; `break`/`continue` do not run them.
  Recursive/nested calls have independent stacks. Unreached registrations do
  not exist. Registration is committed only after all operands succeed.
- Return expressions are evaluated and saved first. Defers then run, followed
  by remaining lexical temporary/local cleanup and the return. Locals belonging
  to blocks already exited have already dropped; saved deferred references stay
  live. There are no named return variables, mutable-reference captures or
  deferred writes into a saved return slot.
- Ordinary functions, closures, direct intrinsics and existing exact raw extern
  calls are supported under their existing ABI rules. Call results are discarded
  (managed results are released). A deferred function returning `Err` does not
  replace the pending outcome. Explicitly handle it in a wrapper if required.
- Use `defer fn(){ statements }();` for an action body. Existing immutable-value
  closure capture applies; `defer { ... }`, `using`, `panic`/`recover`, exception
  unwinding and task cancellation are not introduced. JSON owners/views cannot
  be saved as deferred arguments/results or captured, even if apparently local.
  Raw foreign pointer/text lifetimes remain the caller's unsafe obligation.
- `defer` is contextual. Existing calls/values/assignments named `defer` remain
  valid, including `defer (arg)` as an ordinary call. Consequently a deferred
  callee must not start with grouping parentheses: use `defer f(args);`,
  `defer factory()(args);` or the immediately invoked lambda spelling above.
- Arithmetic/assert/bounds/OOM errors, foreign termination and stack exhaustion
  remain terminal. No defer or remaining local cleanup is guaranteed on a trap,
  including a trap while evaluating a return/registration or invoking a defer.
- Runtime registration is bounded per invocation: 4,096 records / 8 MiB by
  default. Exceeding either budget or allocation failure terminates with a
  located defer error (72), never silently dropping registrations. Records use
  the existing environment allocator domain and typed compiler-generated
  capture/invoke/drop helpers. No erased function-pointer or foreign ABI cast.
  Bytes include each record and its immediate typed capture environment, not
  transitively shared objects. `ON1X_DEFER_MAX_RECORDS` / `ON1X_DEFER_MAX_BYTES`
  are runtime-build test/configuration hooks; exhaustion reports the registration
  file, line and byte column. All operands finish before registration allocation.

## Typed `?` contract

- `value?` evaluates one nominal result once. `Ok(payload)` produces its
  payload. `Err(error)` returns an error result from the innermost source
  function/lambda and runs that function's registered defers. Later operands
  are skipped; earlier evaluated RC operands and lexical JSON owners still
  receive ordinary early-return cleanup. Short-circuiting is unchanged.
- The admitted protocol is a nominal enum with two parameters and exactly
  `Ok(T)` / `Err(E)` payloads. The enclosing return uses the **same declaration**
  and exact error type; its success type may differ. No implicit error conversion,
  name-based module conflation, structural-sum coercion or `Option` propagation.
  Existing generic inference applies; annotate ambiguous result inputs/returns.
  Payloads retain nominal data's trivial-value restrictions; use `record{}` as
  a unit success payload, not `void` (which is not a data field).
- Use explicit `match` for recovery. This is a typed-result alternative to
  exceptions, not `try`/`catch`/`throw`/`finally`. Foreign statuses must be explicitly
  converted to checked result values by existing raw wrappers. No foreign
  safety admission follows from `?`.
- Propagation is expansive/effectful for the value restriction. It lowers to
  existing typed regions, tag tests, payloads and return edges. Tag dominance,
  initialization, function boundaries and exact return types are verified; no
  new scalar MIR encoding is invented for results or dynamic defer effects.
  Unsupported MIR/SDK effect/aggregate profiles reject explicitly.
- Legacy `match(value){Tag(payload)=>expr,...}` retains its existing implicit
  payload-lambda semantics; `?` there cannot escape to the surrounding function.
  Use `match value with {...}` for propagation from an arm/guard into that function.

## Implementation and validation

`src/canonical/values.cpp` erases source propagation into existing value regions,
tag tests, projections, storage initialization and returns. A defer becomes an
ordered block of immutable operand snapshots and a private `PushDefer` of a typed
zero-argument thunk. Ordinary checking recomputes closure captures. The source
PPX wire transports only `Defer` / `Propagate`, not private registration or proof
nodes. Both scanner modes recognize `?` and have parity coverage.

`src/codegen.cpp` emits a stack only for functions actually registering defers.
Compiler-generated invocation/destruction helpers use exact capture types and
the existing RC rules; the runtime allocates records/captures in the environment
allocator domain. It checks count, byte and size-overflow bounds before allocation
and frees the pending record if capture allocation fails. Every successful
invocation destroys/releases its saved values, including discarded managed call
results. No `longjmp`, foreign function-pointer cast, LibStone application runtime
or new execution IR is introduced. Rebuild generated C/runtime together:
`ON1X_DEFER_ABI_VERSION=1` is internal, not a stable embedding ABI.

Tests: `on1x.defer_error_hir` and `on1x.defer_errors`, plus scanner/control and the
existing language suites. They cover LIFO/outcome Python oracles at C `-O0/-O2`,
registration/return order, loops/recursion/closures, modules/generics/macros/PPX/
staging, abandoned operands, counted RC/JSON cleanup, C/C++ separate-TU callers,
record/capture allocation failures, both budgets, fatal paths, source-wire/HIR
mutations and explicit MIR/SDK rejections. Separate-TU tests use a shared generated
interface: they do not claim separate On1x compilation or aggregate foreign ABI.

Implementation/validation status: `capability-matrix.md` and `../VERIFICATION.md`.
