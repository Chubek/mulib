# virtio-guest

MuLIB VMM's **frontend**. Implementation lives in
[`include/mulib-virtio-guest.h`](../../include/mulib-virtio-guest.h).
This directory provides the public entry point and optional compiled translation unit.

Link CMake target `mulib_virtio_guest`, or use the implementation switches from
[the manual](../../manual/mulib-virtio-guest.md). Supply a synchronous transport
adapter and guest-addressed workspace. The executable
[`virtio-guest` example](../../examples/modules/virtio-guest.c) connects both sides.

The [protocol and support matrix](../../MULIB_VMMPROTO.md) are shared with the
backend. This library does not yet provide a native kernel/hypercall binding.
