# Cooperative fibers and typed directional channels

This is the next vertical implementation of section 2 of
`../ON1X_CORE_SYNTAX_ADDITIONS.md`, not completion of that specification.
The earlier Go-style function-scoped defer contract is unchanged.
The [cancellation/deadline continuation](cancellation-deadlines.md) extends this
profile with running/blocked cancellation, monotonic `poll after`, and
`taskgroup within`. Async/await remains unsupported.

## Selected profile

- Linux/glibc C11, one native thread, the existing MuLIB N:1 stackful fiber
  scheduler and channel kernels. No second scheduler/channel implementation,
  C++ coroutine interpreter or LibStone application runtime.
- `taskgroup { statements }` establishes a lexical owner. `launch { ... }`
  eagerly schedules an internal fiber; its body has an i64 result, using a final
  unterminated expression or explicit returns. `ifiber<i64>` is affine:
  `join handle` consumes it and yields i64. Every reached handle must be joined
   or consumed by `cancel` on every normal exit; no detach or silently dropped
   unfinished child.
- `taskgroup within duration { statements }` additionally limits the body and
  its descendants by a monotonic deadline. Expiry stops the run cooperatively,
  drains cleanup and exits 73 with the timed group's source location. It is
  not a branch, a recoverable join result, or a substitute for normal affine
  consumption. Use `poll after` for a recoverable choice. See the
  [deadline contract](cancellation-deadlines.md#taskgroup-within).
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
  respectively. These stream endpoints use the same structural directional
  types as general `u8` channels but remain tied to the process-pipe adapter.
  Sending to stdin returns the generic checked
  `sum{Err:i64,Ok:record{}}` send shape; `close input` is
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
  Atomic `poll` may mix these byte endpoints with general typed channels; each
  receive binder retains its exact payload type (see
  [stream poll](stream-poll.md)). Closing a stdout/stderr receiver and copying
  or reversing endpoint directions are rejected. Pipe `EPIPE` closes stdin, so a
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
- `cancel child` consumes and joins an owned internal fiber. Before dispatch it
  skips the body and releases the unstarted environment. For a running or blocked
  child it requests cancellation, withdraws interrupted waits, runs registered
  function defers LIFO and lexical cleanup through explicit generated returns,
  and cancels/drains descendants. It returns true if requested before completion,
  false for an already completed child. Cleanup is shielded, not forcibly
  interrupted; an infinite/blocking cleanup can prevent cooperative completion.
  Cancellation is observed at generated function entry, loop backedges and
  call/communication resumption. Already committed effects are not rolled back.
- Immutable fixed-size pointer-free values and scoped directional endpoints may
  be captured. An endpoint capture moves that capability out of the parent.
  Mutable, JSON, managed, function-valued and fiber-handle captures are rejected.
  Ordinary closures and function signatures cannot export concurrency handles.
- `->(S,R){payload:T,capacity:N,mode:uni}` creates a scoped `channel<T>`
  descriptor. `S` and `R` are positive decimal literals, each at most 256 and
  with `S + R <= 256`; `N` is a literal in 0..256. The exact quotas are retained
  in checked descriptor types. `split descriptor` consumes it and must bind
  exactly `S + R` simple names through an immutable let pattern: all
  `sender<T>` endpoints first, then all `receiver<T>` endpoints. Endpoints are
  separately affine capabilities, not aliases or cloneable views.
- `T` may be a closed fixed-size pointer-free value whose conservative generated
  representation is at most 32,768 bytes: numeric scalars, bool/char, tuples,
  structural or nominal records/sums, fixed arrays and snapshot slices composed
  from those values. Strings, managed `sexpr`/buffers/callables, JSON/regex
  resources or views, `own<T>`, concurrency capabilities, unresolved types and
  larger layouts reject at the payload annotation with a source location.
  MuLIB copies exactly `sizeof(T)` bytes; there is no widening, serialization,
  retain/drop callback or cleanup transfer.
- `(tx <- value)` returns the checked structural outcome
  `sum{Err:i64,Ok:record{}}`; 1 denotes closed. Handle it with `match` or an
  explicit expression discard. Bare `tx <- value;` rejects until channel-error
  effects can propagate through function signatures. This is an explicitly
  bounded typed-result profile, not nominal `Result`/`?` admission.
- `<-rx` returns `sum{None:record{},Some:T}`. Close drains queued messages
  before `None`. `close tx` is checked and idempotent for that sender endpoint
  (bool: first endpoint close versus repeated close). The underlying channel
  closes only when its last sender endpoint closes. Dropping an endpoint does
  not implicitly close it; use `close` for prompt EOF. The group closes and
  destroys its channel storage after all children finish, including early exits
  after their required joins.
- `try_send(tx, value)` never parks and returns
  `sum{Closed:record{},Sent:record{},WouldBlock:record{}}`.
  `try_recv(rx)` returns
  `sum{Closed:record{},Empty:record{},Value:T}`. A drained close is distinct
  from an empty open channel. Operands still evaluate exactly once.
- Capacity zero is an acknowledged rendezvous; positive capacity is bounded FIFO.
  Channel operations and joins suspend only the current fiber. Loop backedges
  yield cooperatively. No implicit blocking-I/O interception or preemption.
- `poll { recv rx as n => expression, closed rx => expression, send tx <-
  value => expression, closed tx => expression, default => expression }`
    selects among 1..16 distinct named directional endpoints, including mixed
    payload types and process streams. Statement arms may use blocks;
  expression arms must have a common result. Each offered endpoint has exactly
  one `closed` arm; at most one `default` is permitted. Send payloads evaluate
  left-to-right, exactly once, **before** selection; only the winning send
  commits. The MuLIB selection registers all pending cases under one lock,
  commits at most one, and withdraws losing registrations before an arm runs.
  `default` never suspends; without it a fiber may park. A drained closed
  receive or closed send selects its matching `closed` arm. The kernel rotates
  the first probed case. Private timer/cancellation cases participate in that
  rotation, so this is bounded rotating priority rather than strict alternation
  or a general scheduler starvation guarantee. As with other operations,
  allocation or terminal scheduler failure is not recoverable or unwound. One optional
  `after duration => arm` (milliseconds, or `ms`/`s` units) adds a monotonic
  timer offer; a timer-only poll is supported. Durations evaluate once after
  send operands, and an expired timer competes atomically with communications.
  Default never suspends. See the deadline contract for exact arbitration.
- All concurrency values are group-owned, never scalar foreign handles. No
  aliases, assignment, user-built aggregates (except compiler-owned split
  tuples and the four-capability captured-process result), return, ordinary
  closure escape, foreign ABI or deferred retention. Branch joins and repeatable loop
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

Typed cancelled join results, the full portable shell grammar
(expansion, environment mutation, command lists,
pipelines/redirection and portable builtins), duplex channels, endpoint
cloning, managed/resource-bearing payload transfer,
async/await, generators, actors, native worker threads, atomics/shared state and
general error/effect inference remain separate capabilities. Source operations
outside this profile must reject, never block a native thread as a fake fiber.
The scalar MIR and independent SDK adapters reject concurrency operations/types.
Raw foreign declarations and unmodeled blocking intrinsics reject in a concurrent
unit; ordinary nonconcurrent source behavior is unchanged.

MuLIB's park/select timeout API still uses UTC. On1x does not use those timed
waits for language deadlines: its existing supervisor observes CLOCK_MONOTONIC
and commits a private timer-channel offer. Private cancellation-channel offers
also wake blocked channel/select waits without changing kernel semantics.
Its stackful
runtime has fixed stacks and no TSan fiber annotations; no cross-platform,
stack-exhaustion safety or native-thread race-freedom claim follows from this
single-thread profile. ASan's fiber-switch hooks are used by the actual kernel.

## Source-profile status

| Capability | State | Remaining gate |
|---|---|---|
| `taskgroup`, internal i64 `launch`/`join` and cooperative consuming `cancel` | EXECUTABLE | Automatic normal-exit joins, native-worker ownership, effectful MIR and independent cross-platform verification |
| `taskgroup within` | EXECUTABLE terminal-timeout C11 profile | Typed timeout propagation/recovery and effectful MIR |
| Typed uni channels, exact directional quotas, bounded/zero-capacity send/receive/try/close | EXECUTABLE fixed-size pointer-free C11 profile | Managed/resource payload protocols, endpoint cloning/duplex, native workers and effectful MIR |
| `poll` / atomic select | EXECUTABLE heterogeneous typed profile, monotonic `after` and cancellation-aware waits | Recoverable scheduler/allocation errors, managed transfers and strict scheduler-wide fairness |
| Direct-exec external `xfiber` and consuming `cancel` | EXECUTABLE bounded POSIX profile | Typed exit-status record, recoverable spawn errors and broader targets |
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
