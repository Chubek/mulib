# Cooperative cancellation and monotonic choice deadlines

Implementation task: running/blocked internal-fiber cancellation and `poll after`
on the existing single-native-thread C11 profile. Async frames are a separate
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

Taskgroup `within`, typed cancelled join outcomes and general async/effect MIR
remain separate unless explicitly admitted in the verification record.

## Compatibility and bounds

The private CSP runtime ABI is v3: rebuild generated C and its adapter together;
the compiler rejects a header with an incompatible ABI. The legacy
`on1x_csp_cancel_ready` symbol delegates to consuming cooperative cancellation.
Source programs that used immediate cancellation keep their behavior. A
completed internal cancellation now returns false rather than failing.
The PPX-v1 `SourceCsp` operation vocabulary adds `offer_after`, not trusted
lowered nodes or type facts; duplicate/malformed deadline carriers reject.
No scanner token or grammar delimiter changes.

Each bounded task owns one private cancellation channel; at most one selection
per task owns a timer. Existing task/channel/group/work budgets remain in force.
Compiler cleanup expansion has a 262,144-unit budget (each emitted edge plus
its temporary/lexical releases), with a located CSP limit error on exhaustion;
failed compilation does not publish partial C.
Timer/cancellation storage and losing observers are reclaimed before scope
destruction. OOM and other terminal failures remain non-unwinding. This is
EXECUTABLE for Linux/glibc x86_64, not a general async, exception, native-thread
or independent SDK proof profile.
