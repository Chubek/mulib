# Initial cooperative memory model

The admitted G4 profile executes all On1x fibers on one native thread.
Scheduling points are channel send/receive, join and loop backedges. Ordinary
expression operands still evaluate once, left-to-right; suspension does not
reevaluate an operand. MuLIB's channel lock commits the communication and copies
one i64 before returning success. Rendezvous success acknowledges a receiver.

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
worker thread. No On1x heap pointer, closure environment or borrowed endpoint
crosses the process boundary; only checked constant argv strings are copied by
`posix_spawn`. The supervisor observes process completion with `waitpid` and
wakes the owning fiber before `join`/`cancel` returns. Inherited stdout/stderr
may interleave with On1x output; no ordering is promised except that a joined
process has terminated and been reaped. Immediate pre-dispatch internal
cancellation skips the body and releases its unstarted closure environment;
running/blocked cancellation remains unsupported, including attempts to unwind
across channel waits.
