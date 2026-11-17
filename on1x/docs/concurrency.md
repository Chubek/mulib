# Cooperative fibers and channels — initial G4 contract

This is the next vertical implementation of section 2 of
`../ON1X_CORE_SYNTAX_ADDITIONS.md`, not completion of that specification.
The earlier Go-style function-scoped defer contract is unchanged.

## Selected profile

- Linux/glibc C11, one native thread, the existing MuLIB N:1 stackful fiber
  scheduler and channel kernels. No second scheduler/channel implementation,
  C++ coroutine interpreter or LibStone application runtime.
- `taskgroup { statements }` establishes a lexical owner. `launch { ... }`
  eagerly schedules an internal fiber; its body has an i64 result, using a final
  unterminated expression or explicit returns. `ifiber<i64>` is affine:
  `join handle` consumes it and yields i64. Every reached handle must be joined
  or immediately cancelled on every normal exit; no detach or silently dropped
  unfinished child.
- `launch exec("/absolute/program", "literal-arg", ...)` eagerly starts one
  external process with POSIX `posix_spawn` and a fresh process group. The
  first argument is the absolute executable path; all 1..9 argv entries are
  bounded source string literals. This is **direct exec, not shell syntax**:
  no command parsing, interpolation, pipeline, redirection or `/bin/sh -c`.
  Environment and standard streams are inherited, not captured as typed
  streams. The handle type is `xfiber`, affine and scoped to its taskgroup.
  `join job` consumes it, reaps the process and returns an i64: 0..255 for an
  exit code, or negative signal number for signal termination. The exit-status
  convention is a bounded POSIX profile, not the proposed `exit_status`
  record/stream API. The runtime caps total launches at 128 per execution;
  spawn/OOM failures are located terminal errors (exit 73), not typed results.
  Language-profile migration G4/v2: the previously available nominal type
  spelling `xfiber` is now reserved for this handle. Programs declaring a
  nominal `xfiber` must rename it; they receive an explicit diagnostic rather
  than silently being given a process capability.
- `cancel job` **for an `xfiber`** consumes the handle, sends SIGTERM to
  its process group if still live, then awaits and reaps that process. If it
  has not exited after 250 monotonic milliseconds the supervisor sends
  SIGKILL. It returns `true` when SIGTERM was requested, `false` if the process
  had already exited; repeated cancel/join is rejected statically. Cleanup
  completes before the call returns, and a taskgroup cannot leave with an
  unjoined/uncancelled process. OS-level uninterruptible waits or independently
  escaped descendants cannot be proven to stop; no detached-process API exists.
- `let child = launch { ... }; cancel child;` is the **only** internal-fiber
  cancellation spelling admitted here. The `cancel` must be the very next
  statement in the same block: the single-threaded scheduler cannot have run
  that child yet. It consumes and joins the ready fiber without entering its
  body; the unstarted closure environment is released. It returns `true` but
  the standalone statement discards that result. No body defer has registered
  yet. Any intervening statement, nested expression or yield rejects at
  compile time. This is pre-dispatch cancellation, **not** a way to interrupt
  a running fiber, channel wait, `poll`, or nested function/cleanup edge.
- Immutable i64 values and scoped channel endpoints may be captured. An endpoint
  capture moves that capability out of the parent. Mutable, JSON, managed,
  function-valued and fiber-handle captures are rejected in this first profile.
  Ordinary closures and function signatures cannot export concurrency handles.
- `->(1,1){payload:i64,capacity:N,mode:uni}` creates a scoped descriptor;
  `N` is a literal in 0..256. `split descriptor` consumes the descriptor and
  must bind exactly `(sender,receiver)` through an immutable let pattern.
  Cardinalities other than one, duplex, cloning and general payloads reject.
- `(tx <- value)` returns the checked structural outcome
  `sum{Err:i64,Ok:record{}}`; 1 denotes closed. Handle it with `match` or an
  explicit expression discard. Bare `tx <- value;` rejects until channel-error
  effects can propagate through function signatures. This is an explicitly
  bounded typed-result profile, not nominal `Result`/`?` admission.
- `<-rx` returns `sum{None:record{},Some:i64}`. Close drains queued messages
  before None. `close tx` is checked/idempotent (bool: first close versus already
  closed). Dropping an endpoint view does not implicitly close it; use `close`
  for EOF. The group closes/destroys its channel storage after all children
  finish, including early exits after their required joins.
- Capacity zero is an acknowledged rendezvous; positive capacity is bounded FIFO.
  Channel operations and joins suspend only the current fiber. Loop backedges
  yield cooperatively. No implicit blocking-I/O interception or preemption.
- `poll { recv rx as n => expression, closed rx => expression, send tx <-
  value => expression, closed tx => expression, default => expression }`
  selects among 1..16 distinct named endpoints. Statement arms may use blocks;
  expression arms must have a common result. Each offered endpoint has exactly
  one `closed` arm; at most one `default` is permitted. Send payloads evaluate
  left-to-right, exactly once, **before** selection; only the winning send
  commits. The MuLIB selection registers all pending cases under one lock,
  commits at most one, and withdraws losing registrations before an arm runs.
  `default` never suspends; without it a fiber may park. A drained closed
  receive or closed send selects its matching `closed` arm. The kernel rotates
  the first probed offer; this is fairness among initially ready offers, not
  a general starvation guarantee. As with other operations, allocation or
  terminal scheduler failure is not recoverable or unwound.
- All concurrency values are group-owned, never scalar foreign handles. No
  aliases, assignment, aggregates (except split's pair), return, ordinary closure
  escape, foreign ABI or deferred retention. Branch joins and repeatable loop
  transfers are conservatively checked. Typed canonical operations remain
  explicit effects, never erased into ordinary raw calls.
- A bounded supervisor detects quiescent deadlock and scheduler work exhaustion
  as located terminal concurrency failures, not successful completion. Allocation,
  runtime protocol and terminal source failures do not promise unwinding. Before
  a terminal runtime exit, the adapter sends SIGKILL to known live external
  process groups; it does not claim to reap them or run On1x cleanup frames.

## Deliberately unsupported

Running/internal-blocked-fiber cancellation and cancellation unwinding, external shell literals
and byte-stream capture, duplex/MPMC
endpoint cloning, general/managed payloads and results, `poll after` deadlines,
async/await, generators, actors, native worker threads, atomics/shared state and
general error/effect inference remain separate capabilities. Source operations
outside this profile must reject, never block a native thread as a fake fiber.
The scalar MIR and independent SDK adapters reject concurrency operations/types.
Raw foreign declarations and unmodeled blocking intrinsics reject in a concurrent
unit; ordinary nonconcurrent source behavior is unchanged.

MuLIB's current park/select deadlines use UTC, not monotonic time. Its stackful
runtime has fixed stacks and no TSan fiber annotations; no cross-platform,
stack-exhaustion safety or native-thread race-freedom claim follows from this
single-thread profile. ASan's fiber-switch hooks are used by the actual kernel.

## Source-profile status

| Capability | State | Remaining gate |
|---|---|---|
| `taskgroup`, internal i64 `launch`/`join` and immediate pre-dispatch `cancel` | EXECUTABLE | Running/blocked cancellation, native-worker ownership, effectful MIR and independent cross-platform verification |
| 1:1 i64 uni channels, bounded/zero-capacity send/receive | EXECUTABLE | General messages, endpoint quotas/duplex, cancellation and complete sanitizer/stress matrix |
| `poll` / atomic select | EXECUTABLE bounded uni/i64 profile | Cancellation, monotonic deadline, general payloads and recoverable scheduler errors |
| Direct-exec external `xfiber` and consuming `cancel` | EXECUTABLE bounded POSIX profile | Portable shell grammar, typed exit-status/streams and internal-fiber cancellation |
| Async/await, actors, native workers | Unsupported | Suspension frames, ownership/effects and memory model |

Compatibility: `split!(...)` remains a macro call and `if close { ... }` uses
the existing identifier `close`. `match <-rx with { ... }` treats `<-` as a
receive prefix at the discriminant start. Both scanner modes have the same
tokens. The standalone PPX v1 schema copy adds the corresponding source-only
constructors; these carry no checker authority. Neither the semantic C11
backend nor the existing scalar MIR guesses missing concurrency behavior.

Implementation and measured test status: `capability-matrix.md` and
`../VERIFICATION.md`.
