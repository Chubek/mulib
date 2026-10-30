# ocicontainervfs

`mount` routes a memory rootfs into a VFS, disabling the write operation when
readonly=1. Returns 0 success or VFS error. Root must be initialized and outlive
the mount; router borrows its backend context. Unmount/free router before root.

This is an in-memory routing policy, not chroot, native mount namespace isolation
or defense against clients that already hold the underlying mutable root object.
Switches: `MULIB_IMPL_OCICONTAINERVFS`, `MULIB_IMPL_VFS`, `MULIB_IMPL_ROOTFS`,
`MULIB_IMPL_ARCHIVE`, `MULIB_IMPL_BUFFER`.
Example: `examples/modules/ocicontainervfs.c`.
