# ocicontaineripc

Binary channel messages over shared protocol frames. `send` uses IPC opcode and
request-ID field as uint64 channel. Payload may contain NUL, <=1 MiB. Returns 1
success, 0 invalid/OOM. `receive` requires IPC opcode and returns borrowed payload,
channel, length and consumed size only on success: 1 frame, 0 incomplete, -1
invalid/corrupt/wrong opcode. Input must outlive payload use.

No native IPC namespace, message queue, socket, authorization or encryption.
Switches: `MULIB_IMPL_OCICONTAINERIPC`, `MULIB_IMPL_CONTAINER2DAEMONPROTO`,
`MULIB_IMPL_BUFFER`, `MULIB_IMPL_BYCUTILS`, `MULIB_IMPL_CRC32`.
Example: `examples/modules/ocicontaineripc.c`.
