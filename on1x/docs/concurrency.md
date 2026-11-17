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
  bounded source string literals. Environment and standard streams are
  inherited. ``launch `/absolute/program "one word" escaped\ word``` is the
  compiler-parsed command-literal spelling for the same direct-exec behavior.
  Single backticks are one line; triple backticks may span lines. Whitespace
  separates words, single/double quotes retain whitespace, and backslash quotes
  the next non-newline byte. Quotes and escapes are removed before spawn. `#`
  has no comment meaning inside the literal. Empty quoted words are retained.
  The executable must still be an absolute literal path. There are at most nine
  words, 4,096 bytes per word and 16,384 decoded bytes in total, including one
  terminating NUL per argv entry.
  This is a **bounded portable command subset, not a host shell**: `$` expansion,
  assignments/`export`, `;`, `&&`/`||`, pipes, redirections and multiple commands
  reject. Neither `/bin/sh -c` nor `cmd.exe /c` is used. Executable paths remain
  platform-specific.
  The handle type is `xfiber`, affine and scoped to its taskgroup.
  `join job` consumes it, reaps the process and returns an i64: 0..255 for an
  exit code, or negative signal number for signal termination. The exit-status
  convention is a bounded POSIX profile, not the proposed `exit_status`
  record/stream API. The runtime caps total launches at 128 per execution;
  spawn/OOM failures are located terminal errors (exit 73), not typed results.
  Language-profile migration G4/v2: the previously available nominal type
  spelling `xfiber` is now reserved for this handle. Programs declaring a
  nominal `xfiber` must rename it; they receive an explicit diagnostic rather
  than silently being given a process capability.
- ``let (job, input, output, error) = launch capture `/absolute/program ...`;``
  uses the same command grammar but replaces the child's standard descriptors
  with three checked pipes. Its affine result type is
  `(xfiber, sender<u8>, receiver<u8>, receiver<u8>)`: stdin, stdout and stderr
  respectively. The byte endpoints are process-stream capabilities, not a way
  to construct arbitrary `u8` channels. Sending to stdin returns the same
  checked `sum{Err:i64,Ok:record{}}` shape as an i64 send; `close input` is
  idempotent and produces child EOF after queued bytes drain. Joining or
  cancelling also closes stdin. Receiving either output returns
  `sum{None:record{},Some:u8}` and returns `None` only after pipe EOF and queued
  bytes drain. Bytes, including NUL, are not decoded as UTF-8.
  Each stream channel has fixed capacity 256 by default (test builds may lower
  `ON1X_CSP_STREAM_CAPACITY`); the nonblocking supervisor transfers bytes between
  these channels and POSIX pipes. Full channels apply backpressure without
  blocking the native scheduler thread. A program expecting more than the
  bounded capacity must drain stdout/stderr while the process runs—typically by
  moving each receiver into an internal fiber—before joining the process.
   Atomic `poll` may mix these byte endpoints with i64 channels; each receive
   binder retains its exact payload type (see [stream poll](stream-poll.md)).
   Closing a stdout/stderr receiver and copying or
  reversing endpoint directions are rejected. Pipe `EPIPE` closes stdin, so a
  subsequent or still-blocked send yields checked failure instead of terminating
  the On1x process (an already enqueued buffered send may have returned `Ok`).
  The adapter blocks and consumes a newly generated SIGPIPE only around each
  captured-stdin write, restoring the prior thread signal mask before another
  fiber runs; it does not replace the process-wide disposition. Spawned children
  start with the default SIGPIPE disposition.
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
- Immutable i64 values and scoped i64/process-stream channel endpoints may be
  captured. An endpoint capture moves that capability out of the parent. Mutable, JSON, managed,
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
   selects among 1..16 distinct named i64 or captured-process u8 endpoints,
   including mixed choices. Statement arms may use blocks;
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
  aliases, assignment, aggregates (except the compiler-owned split pair and
  four-capability captured-process result), return, ordinary closure
  escape, foreign ABI or deferred retention. Branch joins and repeatable loop
  transfers are conservatively checked. Typed canonical operations remain
  explicit effects, never erased into ordinary raw calls.
- A bounded supervisor detects quiescent deadlock and scheduler work exhaustion
  as located terminal concurrency failures, not successful completion. Allocation,
  runtime protocol and terminal source failures do not promise unwinding. Before
  a terminal runtime exit, the adapter sends SIGKILL to known live external
  process groups; it does not claim to reap them or run On1x cleanup frames.

The executable stream shape is:

```on1x
taskgroup {
    let (process, input, output, error) = launch capture `/bin/cat`;
    let sent = input <- 65u8;
    match sent with { Ok(_) => assert(true), Err(_) => assert(false) };
    close input;
    match <-output with { Some(byte) => println(u8_to_bits(byte)), None(_) => println(-1) };
    match <-output with { Some(_) => println(-2), None(_) => println(0) };
    match <-error with { Some(_) => println(-3), None(_) => println(0) };
    println(join process);
}
```

## Deliberately unsupported

Running/internal-blocked-fiber cancellation and cancellation unwinding, the full
portable shell grammar (expansion, environment mutation, command lists,
pipelines/redirection and portable builtins), duplex/MPMC
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
| `poll` / atomic select | EXECUTABLE bounded i64/process-u8 profile | Cancellation, monotonic deadline, general payloads and recoverable scheduler errors |
| Direct-exec external `xfiber` and consuming `cancel` | EXECUTABLE bounded POSIX profile | Typed exit-status record, recoverable spawn errors, broader targets and internal-fiber cancellation |
| Backtick command literals and captured process streams | EXECUTABLE bounded POSIX profile | Full portable command AST/operators/expansion, configurable language-level stream limits and effectful ownership MIR |
| Async/await, actors, native workers | Unsupported | Suspension frames, ownership/effects and memory model |

Compatibility: `split!(...)` remains a macro call and `if close { ... }` uses
the existing identifier `close`. `match <-rx with { ... }` treats `<-` as a
receive prefix at the discriminant start. Both scanner modes have the same
tokens. The standalone PPX v1 schema copy adds the corresponding source-only
constructors; these carry no checker authority. Neither the semantic C11
backend nor the existing scalar MIR guesses missing concurrency behavior.

Implementation and measured test status: `capability-matrix.md` and
`../VERIFICATION.md`.
