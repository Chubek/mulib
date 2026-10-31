# mulib-container-stub

## NAME AND INTEGRATION

Nonblocking lifecycle client with partial I/O and request/response correlation.
Define `MULIB_IMPL_CONTAINER_STUB` and `conainerd-ipc` transitive switches.
Example: `examples/modules/container-stub.c` connects it to an in-memory bridge.

## TRANSPORT

Transport contains read/write callbacks and a borrowed user pointer. Callbacks
return OK with a transferred prefix, AGAIN with zero bytes, or negative CT error.
Zero-byte OK is treated as no progress. Counts exceeding requested size, nonzero
counts with errors, and undocumented positive statuses are EIO. Report EOF as a
negative transport error; zero-byte reads mean retry, not EOF.

Callbacks do not retain buffers or reenter the same client. The client has no
socket ownership, implicit polling, reconnect, timeout clock or thread.

## REQUEST STATE MACHINE

`init(client,transport,first_request_id)` requires nonzero ID and both callbacks.
`begin(command)` requires IDLE, copies the command, allocates/encodes its frame
and assigns the next monotonic client ID, replacing command.request_id. IDs do
not wrap. On encoding failure the client remains idle and does not consume an ID.

`pump(io_budget)` performs at most that many callbacks, maintaining partial send
and receive offsets. It returns AGAIN while work remains and DONE when a complete
reply is buffered. Reads request only the current frame's remaining bytes, so the
client cannot overconsume a later frame. Only one request is outstanding.

`take(&status)` checks response flag, opcode and request ID, then returns remote
status and restores IDLE. Successful responses update status; remote errors leave
it unchanged. Correlation/malformed-success failures become sticky FAILED state.
Transport/framing failures also remain sticky rather than resending an operation
whose execution may already have occurred.

## OWNERSHIP AND CANCELLATION

Client owns request and incremental response bytes, and borrows transport context.
`free` releases buffers and resets state. Canceling mid-frame requires closing or
externally resynchronizing the transport; freeing local state cannot undo bytes
already sent or a server operation. A busy free is ignored until callback return.

The underlying protocol and server replay policy are documented in
[conainerd-ipc](mulib-conainerd-ipc.md) and
[ctrlbridge](mulib-containerd-ctrlbridge.md). All results use CT codes.
