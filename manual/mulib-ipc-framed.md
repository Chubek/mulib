# mulib-ipc-framed

## Build
`MULIB_IMPL_IPC_FRAMED`, `MULIB_IMPL_SANDBOX_PROFILE`; POSIX stream sockets.
Linux: `-D_GNU_SOURCE`; on supported non-Linux systems expose POSIX clocks and
SIGPIPE-safe socket flags. Unsupported systems report unsupported.

## Wire protocol
Exactly 4 bytes unsigned big-endian length, followed by that many bytes. Zero
length is valid, no terminator. The maximum allowed frame is at most 64 MiB.
No encoding assumptions or native pointer representations. This is a stream
protocol, not JSON-RPC. Duplicates, schema versions, identity and authorization
are handled at a higher layer. Do not call from multiple threads or interleave
writers.

`mulib_ipc_framed_init` checks a connected stream socket, borrows the descriptor
and establishes SIGPIPE-safe writes. The owner keeps it open and closes it on
failure. `mulib_ipc_now_ms` returns monotonic milliseconds, or -1 on error;
`mulib_ipc_deadline(timeout_ms,&absolute)` checks overflow.
`mulib_ipc_send` and `mulib_ipc_receive` use nonblocking send/recv and poll;
retry EINTR and partial transfer while observing the same absolute deadline.
Receive writes the caller's bounded buffer, never allocates a frame. Its output
length remains unchanged on failure, but destination bytes may be partially
written and must be discarded. Local argument/oversize send rejection does not
consume stream bytes; I/O failure, deadline, truncation, incoming oversize and
EOF poison the stream. Never retry on a poisoned stream; close it. Use one
absolute deadline for an entire job including all broker round trips.
`MULIB_SB_EOF` is orderly peer shutdown before a new header starts; a partial
header or a declared payload cut short is `MULIB_SB_PROTOCOL`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-ipc-framed.h`](../kernels/mulib-ipc-framed.h) |
| Implementation switch | `MULIB_IMPL_IPC_FRAMED` |
| Repository-local includes | [mulib-sandbox-profile](mulib-sandbox-profile.md) |
| Complete example | [`examples/modules/sandbox-process.c`](../examples/modules/sandbox-process.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/sandbox-process.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
