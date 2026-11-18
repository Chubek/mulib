# Cooperative cancellation and monotonic deadlines

The single-native-thread C11 profile supports running/blocked internal-fiber
cancellation, `poll after` and `taskgroup within`. Async frames are a separate
obligation; a stackful fiber is not an async function.

## Selected semantics

`cancel handle` consumes an owned internal handle, requests cooperative
cancellation, and waits for all of that child's cleanup and descendants. It
returns true if cancellation was requested before completion, false if already
complete. No self-cancel, detach, aliases or cross-parent cancellation.
The old immediate pre-dispatch spelling retains its behavior.

Cancellation points are generated function entry, loop backedges and returns
from potentially suspending calls/communications. Pending channel/select
registrations are removed before explicit generated cleanup returns run.
Registered function defers execute LIFO, then lexical owners drop; cleanup is
shielded from repeat cancellation. No longjmp, context abandonment, C++ exception
or forced native-thread termination is permitted. Already committed
communications remain committed even if cancellation is observed on resumption.
Noncooperative foreign/native calls cannot be interrupted.
Owned subprocesses use the existing process-group SIGTERM/250ms/SIGKILL
cancellation and reaping protocol, not a separate unbounded wait.

`poll { ... , after 25ms => ..., default => ... }` evaluates its duration once,
after the source send operands and before registering offers. Units are `ms` and
`s`; unsuffixed expressions denote milliseconds. Negative durations fail with a
located concurrency diagnostic; unit multiplication uses checked i64 arithmetic.
At most
one deadline and one default. A timer-only choice is permitted. Deadline and
communications compete in the same atomic selection, not a readiness/probe
loop. Ready operations, an expired timer and default obey the kernel's normal
ready-before-default policy. `after 0ms` is immediately ready.

The supervisor observes CLOCK_MONOTONIC and closes a private timer channel on
expiry. A blocked selection also offers its task's private cancellation channel.
Thus neither deadline nor cancellation changes MuLIB's existing UTC wait
semantics or duplicates its channel scheduler. Losing registrations are removed
by the kernel before timer/cancel storage is reclaimed. Deadlines are scheduling
deadlines, not preemption guarantees. There are at most 16 source channel offers
plus one timer and one cancellation offer.

## Taskgroup `within`

```on1x
taskgroup within 1s {
    let child = launch { 42 };
    println(join child);
}
```

- The `i64` duration evaluates once in the enclosing scope **before** the new
  scope is entered. No suffix or `ms` means milliseconds; `s` multiplies by
  1,000 with checked i64 arithmetic. Negative durations, clock failure and
  absolute deadline overflow are located terminal CSP errors. `0ms` expires
  before any body statement. The scheduler compares millisecond readings of
  `CLOCK_MONOTONIC`; wall-clock adjustments do not change the deadline.
- The deadline covers the owner while executing the group body and all nested
  scopes, internal fibers and subprocesses. Nesting cannot extend an outer
  deadline. Among active deadlines the earliest wins, with equal absolute
  deadlines attributed to the first entered scope. Normal lexical exit,
  including `return` or loop transfer, removes the deadline. Explicitly
  cancelling a task retires deadlines in that task and its subtree, not an
  enclosing owner's deadline. Normal exits still require explicit consuming
  `join` or `cancel` for every owned handle; there is no auto-join.
- The supervisor checks deadlines even when everyone is parked, and generated
  cancellation checks observe elapsed deadlines on entry, at loop backedges,
  around calls/CSP operations and on scope transfer. Expiry is **terminal for
  the whole run**, not a typed result or a skip to the statement after the
  block. The adapter latches the timed group's location, cancels live
  application tasks (including other branches of the task tree), withdraws
  waits, stops/reaps owned subprocesses and drains shielded defers and lexical
  cleanup. It then reports `ON1X-CSP-RUNTIME: monotonic taskgroup deadline
  expired` at the group's source location and exits 73. A cancelled join value
  or selected arm is never handed back to application code. Committed effects
  stay committed. Use `poll after` if timeout must select an alternative arm.
- Expiry is cooperative, not a preemption or hard wall-time guarantee. Cleanup
  remains shielded, including ordinary work launched to perform that cleanup.
  Timed scopes entered from shielded defers or their cleanup-launched tasks
  fail closed with a located runtime error. Nonterminating/noncooperative
  cleanup, scheduler budgets, OS-level process shutdown limits and other
  terminal faults retain the existing CSP limitations. A cleanup fault can
  terminate the run before the ordinary timeout report.

Typed timeout/cancelled join outcomes and general async/effect MIR remain
unsupported.

## Compatibility and bounds

The private CSP runtime ABI is v4: rebuild generated C and its adapter together.
Generated C rejects an incompatible header and uses the `_v4` scheduler entry
symbol to reject an older runtime object at link time. The legacy
`on1x_csp_cancel_ready` symbol delegates to consuming cooperative cancellation.
Source programs that used immediate cancellation keep their behavior. A
completed internal cancellation now returns false rather than failing.
The PPX-v1 source wire has `SourceCsp(offer_after, ...)` and the additive
`TaskGroupWithin(duration, body, location)` constructor; existing `TaskGroup`
has unchanged arity. Neither carries trusted lowered nodes or type facts.
Malformed carriers reject; no scanner token changes.

Each bounded task owns one private cancellation channel; at most one selection
per task owns a timer. Each timed lexical group registers in a bounded list
inside the existing supervisor and uses no extra channel or timer allocation.
Existing task/channel/group/work budgets remain in force.
Compiler cleanup expansion has a 262,144-unit budget (each emitted edge plus
its temporary/lexical releases), with a located CSP limit error on exhaustion;
failed compilation does not publish partial C.
Timer/cancellation storage and losing observers are reclaimed before scope
destruction. OOM and other terminal failures remain non-unwinding. This is
EXECUTABLE for Linux/glibc x86_64, not a general async, exception, native-thread
or independent SDK proof profile.
