# netfs — remote filesystem over MuLIB FUSE

Define `MULIB_IMPL_VFS`, `MULIB_IMPL_FUSE`, `MULIB_IMPL_ENDIAN`,
`MULIB_IMPL_XTKS`, `MULIB_IMPL_NETFS` before including the header.
See [`examples/modules/netfs.c`](../examples/modules/netfs.c) for a complete
remote mount with an in-process exchange callback, and `tests/test_netfs.c`
for the same operations over sockets with deliberately short reads/writes.

## Composition

On the server, mount storage in `mulib_vfs`, initialize `mulib_fuse`, then pass
complete request bytes to `mulib_netfs_serve`. Every valid request is executed
by `mulib_fuse_dispatch`; read-only policy and filesystem errors propagate.
On the client, initialize `mulib_netfs_t` with an exchange callback, obtain
`mulib_netfs_ops()`, and mount those operations in another `mulib_vfs` with the
client as borrowed context. GETATTR, READ, WRITE and READDIR become remote RPCs.
Direct callers of the returned operations must supply a count for read/write,
an info output for stat, and a visitor for list. Missing required outputs return
EINVAL before allocation, request-ID consumption, or remote side effects.

This uses the repository's typed FUSE/VFS adapter. That adapter is not libfuse
or a `/dev/fuse` kernel transport: mounting here creates a MuLIB VFS mount,
not a host-OS filesystem mount. There are no hidden kernel dependencies.

## Storage and failures

Set a maximum frame size between 64 bytes and 16 MiB. Each client operation
allocates at most two buffers of that size. Contexts, filesystem backends and
streams are borrowed. There is one synchronous request per client; callers
must serialize threads, and callback reentry is rejected. Request IDs start at
one and never wrap. An ID mismatch, invalid response size/type/count or corrupt
listing returns EIO before publishing data or invoking directory callbacks.

READ/WRITE are bounded single calls, not automatic chunk loops. A READDIR must
fit one response or returns ELIMIT without delivering partial entries. The
client validates every listing entry before callbacks; callback cancellation
returns ECANCELED. A failed WRITE may already have executed remotely, so no
automatic retry is performed. Backend authorization and transport protection
are explicit application policy. There is no reconnect, cache consistency,
distributed locking, file removal, rename or metadata mutation protocol.

## Wire protocol, version 1

All integers are unsigned big-endian. Request header (32 bytes):
`MNF1`, opcode u8, three zero reserved bytes, ID u64, offset u64, requested
length u32, path length u32; then path bytes, then exactly length bytes for
WRITE only. Paths must be canonical absolute MuLIB VFS paths, without NUL,
and at most 4095 bytes. GETATTR/READDIR require zero offset and length.

Response header (36 bytes): `MNR1`, status u32 (negated `mulib_vfs_result`,
0..12), ID u64, kind u32, size u64, transferred u32, body length u32.
Error responses have all metadata/body fields zero. GETATTR returns kind/size;
READ returns data; WRITE returns a count. READDIR returns repeated records:
kind u32, size u64, name length u32, name bytes. Names are immediate children,
not paths. Unknown valid opcodes produce ENOTSUP. Malformed envelopes return
0 from `serve` without dispatch; valid envelopes return 1 even for FS errors.

`mulib_netfs_stream_exchange` and `mulib_netfs_stream_serve` add a four-byte
length prefix to each complete body over XTK streams. They require blocking
callbacks. Partial transfer errors, AGAIN, oversize frame or unexpected EOF
poison the stream adapter until the application reconnects/reinitializes it;
they never attempt ambiguous replay. Server stream helpers use two disjoint
caller-provided scratch buffers. Neither helper owns or closes its stream.
