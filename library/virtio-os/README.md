# virtio-os

MuLIB VMM's **backend**. Implementation lives in
[`include/mulib-virtio-os.h`](../../include/mulib-virtio-os.h), consistent with
MuLIB's single-header convention. This directory provides the public entry point
and optional compiled translation units.

Link CMake target `mulib_virtio_os`, or define the implementation switches from
[the manual](../../manual/mulib-virtio-os.md). Both compiled sides share
`mulib_virtio_protocol`; `protocol.c` owns its implementation exactly once.

See [`MULIB_VMMPROTO.md`](../../MULIB_VMMPROTO.md) for the wire contract and the
21-profile/27-unimplemented support matrix. Providers supply real host operations.
