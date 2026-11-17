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

## Defer contract

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
- Arithmetic/assert/bounds/OOM errors, foreign termination and stack exhaustion
  remain terminal. No defer or remaining local cleanup is guaranteed on a trap,
  including a trap while evaluating a return/registration or invoking a defer.
- Runtime registration is bounded per invocation: 4,096 records / 8 MiB by
  default. Exceeding either budget or allocation failure terminates with a
  located defer error (72), never silently dropping registrations. Records use
  the existing environment allocator domain and typed compiler-generated
  capture/invoke/drop helpers. No erased function-pointer or foreign ABI cast.

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
- Use explicit `match` for recovery. This is a typed-result alternative to
  exceptions, not `try`/`catch`/`throw`/`finally`. Foreign statuses must be explicitly
  converted to checked result values by existing raw wrappers. No foreign
  safety admission follows from `?`.
- Propagation is expansive/effectful for the value restriction. It lowers to
  existing typed regions, tag tests, payloads and return edges. Tag dominance,
  initialization, function boundaries and exact return types are verified; no
  new scalar MIR encoding is invented for results or dynamic defer effects.
  Unsupported MIR/SDK effect/aggregate profiles reject explicitly.

Implementation/validation status: `capability-matrix.md` and `../VERIFICATION.md`.
