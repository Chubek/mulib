# G4 continuation: atomic process-stream poll

> Historical increment: this page records the original private ABI v2 stream
> extension. The current [typed-channel contract](concurrency.md) and ABI v5
> supersede its int64 widening and “no general byte channel/deadline” limits:
> process streams now use structural `sender<u8>`/`receiver<u8>` endpoints and
> may share a heterogeneous selection with any admitted pointer-free payload.
> The atomicity, backpressure and EOF statements below remain applicable.

At that increment, the scoped task was to extend cooperative `poll` to captured
process
`sender<u8>` and `receiver<u8>` endpoints, without adding a scheduler, parser,
channel kernel or deadline approximation.

## Semantics and ABI

The existing syntax, 1..16 distinct named endpoint budget, mandatory `closed`
arms, optional `default`, registration-time left-to-right send evaluation and
single atomic MuLIB selection are unchanged. i64 and process-byte offers may
share a choice. Each receive binder has its endpoint's exact payload type:
`i64` or `u8`. All arm results must still unify. Only captured stdin can send;
only captured stdout/stderr can receive. Offers borrow capabilities for the
duration of selection, never copy or retain them after the call returns.

The then-private C11 ABI retained its v2 int64 payload transport: every u8 value
widens exactly. The adapter stages each byte in actual `uint8_t` storage before
passing its address to MuLIB; it never treats a byte within an int64 object as
the message. The selected byte is explicitly widened on return, then converted
to the checked u8 binder by C11 emission. Out-of-range byte sends or wrong
runtime stream directions fail with a located terminal diagnostic.

The kernel commits at most one operation while holding its selection lock,
then removes losing registrations before returning. Buffered stdin selection
commits enqueueing, not OS process consumption. Output close is visible only
after pipe EOF and the channel's queued bytes drain. A closed arm is a distinct
outcome, never an invented zero byte. The existing supervisor handles pipe
backpressure without blocking the scheduler thread.

## Compatibility and limits

That increment made no syntax, token, PPX wire-constructor or C ABI signature
changes. Rebuild the
runtime adapter with the compiler to use the extended profile. Previously
rejected byte polls become accepted only with exact payload/direction checks.
Existing i64 poll behavior and process ownership/join obligations are unchanged.
The example is `examples/cooperative_stream_poll.on1x`.

That increment did not include general byte-channel construction, async/await,
monotonic deadlines, running-fiber cancellation, native threads or effectful
MIR. ABI v5 supersedes the first, third and fourth limits as described above;
async/await, native threads and effectful MIR remain absent. Fatal runtime
failures remain non-unwinding. Linux/glibc x86_64 is the only admitted runtime
target. Validation is recorded in `../VERIFICATION.md`; the full syntax
specification is not complete.
