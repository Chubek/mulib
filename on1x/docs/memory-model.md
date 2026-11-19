# Initial cooperative memory model

The admitted G4 profile executes all On1x fibers on one native thread.
Scheduling points are channel/stream send and receive, blocking `poll`, join and loop backedges;
the supervisor also yields while pumping process pipes. Ordinary
expression operands still evaluate once, left-to-right; suspension does not
reevaluate an operand. MuLIB's channel lock commits the communication and copies
one exactly typed value representation before returning success. Channel payloads
are closed fixed-size pointer-free values of at most 32,768 bytes; a transfer is
an ordinary byte copy of `sizeof(T)`, with no hidden pointer ownership,
serialization, widening, retain or drop. Rendezvous success acknowledges a
receiver; a positive-capacity success acknowledges FIFO enqueueing.

An atomic `poll` may offer heterogeneous general channels and process u8 streams together.
The same kernel lock commits exactly one communication; losing registrations
are removed before its arm executes or stack-local payload storage is released.
Every offer points to an object of its exact generated C type. A byte
send/receive therefore uses actual `uint8_t` storage, never an
endian-dependent byte of an i64 object. Buffered process-stdin
success acknowledges enqueueing, not OS child consumption. EOF/closed is
distinct from a received zero byte. Default commits no channel operation.

There is no shared mutable capture or cross-native-thread ownership transfer.
An immutable admitted value is copied; a scoped endpoint capability moves into one
launched body and cannot also be used by the parent. Each child must finish
before its owning group and channel storage are destroyed. Endpoint pointer
representation is private, cannot cross raw FFI and does not authorize copying.
Each sender endpoint closes once; closing one does not close its siblings. The
channel's close event is linearized when the final sender closes. Values already
in the FIFO drain before receivers observe closed. Receiver endpoints do not
close the channel in this profile.

FIFO is the selected kernel's buffered order, including each sender's accepted
order under multi-sender interleaving. Ready select cases use rotating probe
priority; hidden cancellation/timer cases also participate, so strict
alternation and global ordering between independent fibers are not promised.
Native blocking calls are not
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
running/blocked cancellation now uses explicit generated cleanup returns and
private signal-channel offers. A cancelled wait withdraws its kernel
registrations before lexical storage can be released; a previously committed
message is not rolled back. Defers and descendant draining are shielded from
repeated cancellation. There is no forced C-stack abandonment or native-thread
preemption. Monotonic `poll` deadlines likewise commit through private channel
offers and do not depend on the kernel's UTC timeout API.
