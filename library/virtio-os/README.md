# virtio-os

MuLIB VMM's **backend**. Each supported device has its own canonical `.c` file
in this directory, including configuration validation and queue parsing.
`virtio-os.c` implements shared lifecycle, DMA validation and dispatch;
`internal.h` is private. Host resources are supplied by service callbacks.

[`kernels/mulib-virtio-os.h`](../../kernels/mulib-virtio-os.h) retains the
single-header API and a generated implementation. After changing sources run
`python3 tools/amalgamate_virtio.py`; `--check` verifies reproducibility.

Link CMake target `mulib_virtio_os`, or define the implementation switches from
[the manual](../../manual/mulib-virtio-os.md). Both compiled sides share
`mulib_virtio_protocol`; `protocol.c` owns its implementation exactly once.

See the [Virtio manual](../../manual/mulib-virtio.md) for wire helpers and device
support queries. Providers supply real host operations.
In particular, NSM requires a real security provider, not simulated attestation.
