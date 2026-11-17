# Initial cooperative memory model

The admitted G4 profile executes all On1x fibers on one native thread.
Scheduling points are channel/stream send and receive, blocking `poll`, join and loop backedges;
the supervisor also yields while pumping process pipes. Ordinary
expression operands still evaluate once, left-to-right; suspension does not
reevaluate an operand. MuLIB's channel lock commits the communication and copies
one i64 or process-stream byte before returning success. Rendezvous success
acknowledges a receiver.

An atomic `poll` may offer i64 channels and process u8 streams together.
The same kernel lock commits exactly one communication; losing registrations
are removed before its arm executes. A byte send/receive uses actual uint8
storage, never an endian-dependent byte of an i64 object. Buffered process-stdin
success acknowledges enqueueing, not OS child consumption. EOF/closed is
distinct from a received zero byte. Default commits no channel operation.

There is no shared mutable capture or cross-native-thread ownership transfer.
An immutable scalar is copied; a scoped endpoint capability moves into one
launched body and cannot also be used by the parent. Each child must finish
before its owning group and channel storage are destroyed. Endpoint pointer
representation is private, cannot cross raw FFI and does not authorize copying.

FIFO is the selected kernel's buffered order; no preemptive fairness or global
ordering between independent fibers is promised. Native blocking calls are not
implicitly made asynchronous. General Send/Sync, atomics, locks, shared mutable
values and a multiworker happens-before model are not admitted by this profile.

External `xfiber` execution uses a separate OS process, not another On1x
worker thread. No On1x heap pointer, closure environment or endpoint pointer
crosses the process boundary: `posix_spawn` copies checked constant argv and
captured standard streams copy individual bytes through nonblocking POSIX pipes.
The bounded stdin channel preserves accepted-byte FIFO order; closing it drains
accepted bytes before pipe EOF unless the child has already closed its reader.
Each stdout/stderr pipe preserves its own byte order through its receiver; no
ordering is promised between the two pipes. A drained receiver observes `None`
only after pipe EOF. The SIGPIPE mask used for one parent pipe write is restored
before any fiber can run, so it adds no scheduling-visible signal-mask state.

The supervisor observes process completion with `waitpid` and wakes the owning
fiber before `join`/`cancel` returns. Direct-exec inherited stdout/stderr may
interleave with On1x output; no ordering is promised except that a joined
process has terminated and been reaped. Immediate pre-dispatch internal
cancellation skips the body and releases its unstarted closure environment;
running/blocked cancellation remains unsupported, including attempts to unwind
across channel waits.
