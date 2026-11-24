# Stackless timer frames (direct C11)

Inside a taskgroup, an immutable binding may eagerly schedule a restricted
stackless body. For example:

```on1x
taskgroup {
    let child = launch async {
        let seed = 40;
        await timer(5);
        seed + 2
    };
    println(join child);
}
```

`launch async { ... }` produces an affine `ifiber<i64>` and uses the same
consuming `join` / `cancel` and lexical taskgroup rules as `launch { ... }`.
The async block has at most 128 statements, at least one `await timer(i64);`,
and a final `i64` expression or `return`. Its only other statements are
simple immutable `i64` lets. Expressions are i64 literals, earlier frame
locals, unary `-`, and checked `+`, `-`, `*`, `/`, `%`. There are no captures,
loans, calls, mutable bindings, resource-bearing values, or control branches.
The words `async`, `await` and `timer` are contextual, not globally reserved.

The timer duration is evaluated once on the frame's current step, in
milliseconds with no suffix. Negative durations or monotonic overflow
terminate with a located runtime error at the launch. A zero duration still
yields back to the scheduler. `await timer(...)` never returns a value and is
not allowed outside the body or in an expression. Joining the completed frame
is done by its stackful owner; the frame itself never uses that join path.
There is no async function syntax, await of another task/channel, or
suspension-aware borrowing. MIR and its backends reject timer frames, and
the Cephyr object route opts out of the frame ABI.

## Checked HIR and C11 emission

`SourceCsp` carries the untrusted `launch_async` / `await_timer` forms through
the existing PPX wire. After typing, canonical concurrency lowering rewrites
each direct timer statement to a checked `Csp(frame_sleep, delay, next_state)`
with explicit increasing resume states. Frame initialization starts at state
zero. The independent CSP verifier rechecks the state sequence, pure operand
types, prior initialization of every local read, and the affine outer
binding; it rejects both source-only operations in executable HIR and forged
targets. Each admitted immutable `let` declares a typed slot in source order.
Every slot is retained conservatively until frame drop; no exact liveness
minimization is claimed. Backedges and resources are not in this profile,
so initialization flags and per-state drop glue are unnecessary.

The C11 emitter emits a concrete frame struct, a resume switch and a frame
drop callback. The nullary lambda-shaped source carrier is never emitted as
a callable or launched as a stackful fiber. Only the existing supervisor
dispatches the generated resume function. Authored HIR rewrites cannot bypass
frame admission: a transformed candidate is rechecked before emission.

## Runtime contract

[`on1x_async_frame.h`](../runtime/on1x_async_frame.h) is a private additive v1
ABI in the Linux/glibc x86-64 CSP v5 adapter. It is not a public FFI or
embedding API. C11 code allocates a concrete frame struct with a state
discriminant, typed fields and initialization flags as needed. It supplies a `resume`
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

## Beyond timer-only frames

- Nonlinear async bodies need a typed CFG, initialization and live-slot data
  flow, explicit cancel edges, and state-specific cleanup. Independently
  verify loan, ownership and drop facts before permitting resources, views,
  or calls across a suspension. The runtime does not check raw C frame fields
  for lifetime safety.
- Awaiting a task or channel from a frame requires nonblocking readiness
  registration, ownership of that wait and withdrawal on cancel. Never lower
  it to the stackful join path. Effectful MIR and its products must remain
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

In a configured tree, `ctest --test-dir build/on1x -R '^on1x\.concurrency(_hir)?$'`
runs the existing suites including frame admission, malformed HIR and
emitted-code execution. Run only the frame runtime and source cases with:

```sh
python3 -B on1x/tests/test_async_frame.py build/on1x/on1xc "$PWD/on1x" \
  "$PWD/build/frame-tests" cc
```

The C fixture checks typed state across timer and yield steps, mixed fiber
progress, cancellation before dispatch, during a wait, and from an enclosing
fiber, deadline expiry during wait and during a return step, shielded cleanup,
repeated joins, invalid actions/delays and non-owning use. It wraps
`mulib_fiber_spawn` to verify that the only stacks are those of the existing owner,
supervisor and any explicit stackful witness. A controllable clock covers
edge transitions without wall-time races; a separate case uses the real
monotonic clock. The source test builds [`async_timer.on1x`](../examples/async_timer.on1x)
through `on1x-build.py`, observes timer advances while a peer fiber runs, and
counts real frame alloc/free calls and fiber stacks. It also tests identity
PPX, ready/blocked cancellation, group expiry, and rejection of unsupported
source/loan forms and MIR emission. With ASan/UBSan flags it checks leaks on
successful and cooperatively drained exits; only intentional non-unwinding
protocol fault subprocesses disable leak reporting.
