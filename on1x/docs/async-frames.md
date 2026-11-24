# Private stackless frame runtime (C11)

This is a runtime slice, **not source `async`/`await`**. The compiler still
rejects those forms. No new HIR, PPX/SDK wire, MIR, VM, Archon or Cephyr
capability is admitted. Current checked concurrency HIR models `Csp` and
`CspScope`, but not typed frame slots, resume targets or suspension-live loans.

## Runtime contract

[`on1x_async_frame.h`](../runtime/on1x_async_frame.h) is a private additive v1
ABI in the Linux/glibc x86-64 CSP v5 adapter. It is not a public FFI or
embedding API. C11 code allocates a concrete frame struct with a state
discriminant, typed fields and initialization flags. It supplies a `resume`
function and a `drop` function; the runtime neither interprets the fields nor
allocates a per-frame fiber, stack or cancellation channel.

1. `on1x_async_launch_v1` transfers the initialized frame into the current
   taskgroup. Launch is eager but does not invoke `resume` inline. The existing
   CSP supervisor dispatches at most one step per live frame per sweep, then
   yields to MuLIB's scheduler. Each step returns to the supervisor; the C
   call stack does not carry state between steps.
2. `YIELD` makes the frame eligible on the next sweep. `SLEEP_MS` sets a
   checked relative `CLOCK_MONOTONIC` deadline; zero still returns to the
   supervisor before the next dispatch. A sleeping frame resumes only when
   ready; cancellation drops it without resuming it. Timer waits participate in
   the existing idle and deadlock checks. There is no per-frame native
   blocking wait.
3. `RETURN_I64` prepares the scalar result. The supervisor checks for
   taskgroup expiry before dispatch and after a step. Cancellation before
   dispatch, between steps or observed after a step bypasses further resume
   and discards any uncommitted result. Both terminal paths call `drop` once
   on the initialized portion of the frame and free its storage through that
   callback, then mark the task done and wake its waiter. Already committed
   effects are not undone.
4. A stackful owner consumes the child once with `on1x_async_join_v1` or the
   existing `on1x_csp_cancel`. Typed join distinguishes an i64 result, a
   cancelled child, and interruption of an unshielded owner. Interruption
   withdraws the join without consuming the child; cancellation cleanup on
   group exit still drains it. An ordinary group exit with an unconsumed frame
   fails, as with other children. Nothing detaches or escapes the scope.

Frame code is trusted generated C, not a sandbox. Resume/drop must be finite
and nonblocking. Calling the stackful CSP APIs from a frame fails the existing
current-task check; direct native blocking, raw pointers, or forged state
cannot be excluded by the C ABI. Delays, task and work budgets, and invalid
actions fail with located diagnostics. As with existing CSP protocol faults,
fatal errors do not unwind; cooperative cancellation and deadline expiry do.

## Compiler admission still required

- Settle source syntax and the affine task/await contract with the frontend
  owner. Calls returning a task are not ordinary closure calls, and a
  stackless `await` must not lower to the stackful join above.
- Normalize checked bodies into a typed CFG in canonical HIR. Give each
  suspension an explicit continuation, initialized/live-slot set, and cancel
  edge; independently verify target and slot types, ownership states,
  transfer/drop obligations and source provenance. Reject live loans/views
  and non-suspend-safe resources across suspension, including those hidden by
  aggregates, captures or calls. This adapter does **not** check C frame
  fields for loan safety. A first source slice can restrict spill slots to
  closed pointer-free types, track initialization explicitly, and permit only
  compiler-proven cleanup.
- Emit the checked state discriminant, typed fields, resume switch and
  state-specific reverse cleanup in direct C11. This slice only waits on a
  timer; awaiting a task or channel from a frame still requires nonblocking
  readiness registration, ownership of that wait, and withdrawal on cancel.
  Add source success/compile-fail tests, forged HIR rejection, and differential
  lifecycle tests before admitting it. Keep effectful MIR and its products
  unavailable until independently verified and executable.

At pinned LibStone `3be5e9d7ffde4f97f333365e7080e316396a0e55`,
Plaid `weaver/AsyncAnalysis.hpp` computes backward may-liveness and returns
string names/slots. It has no On1x lexical identity, type, initialization,
loan or drop proof. If reused, it needs a projection from checked identities
and independently verified suspension facts. Plaid `loom/Coroutine.hpp`
uses compiler-generated C++20 host coroutine frames; `runtime/PlaidRuntime.h`
exposes a callback queue, not the CSP ownership/cancellation protocol.
Neither is linked into this C11 runtime.

## Reproduce

In a configured tree, `ctest --test-dir build/on1x -R '^on1x\.concurrency$'`
runs the fixture with the existing concurrency suite. Run only the new cases
with:

```sh
python3 -B on1x/tests/test_async_frame.py build/on1x/on1xc "$PWD/on1x" \
  "$PWD/build/frame-tests" cc
```

The C fixture checks typed state across timer and yield steps, mixed fiber
progress, cancellation before dispatch, during a wait, and from an enclosing
fiber, deadline expiry during wait and during a return step, shielded cleanup,
repeated joins, invalid actions/delays and non-owning use. It wraps `mulib_fiber_spawn` to
verify that the only actual stacks are those of the existing owner,
supervisor and any explicit stackful witness. A controllable clock covers
edge transitions without wall-time races; a separate case uses the real
monotonic clock. The same runner checks that neither C11 nor MIR emission
accepts source async forms. With ASan/UBSan flags it checks leaks on successful
and cooperatively drained exits; only intentional non-unwinding protocol
fault subprocesses disable leak reporting.
