# rootfsbuilder

`export` appends a complete USTAR archive from a memory rootfs without consuming
it. Entries follow storage insertion order. File mode 0644, directory mode 0755,
uid/gid/mtime zero; source metadata is not preserved. Paths must fit the existing
USTAR writer profile. Returns 0 success, negative invalid/archive/OOM, atomically.

Archive bytes are caller-buffer-owned. No native filesystem walking, compression,
whiteout layering, hardlinks/symlinks or image signing. See [container limits](extension-layers.md#containers-and-security-sensitive-adapters).
Switches: `MULIB_IMPL_ROOTFSBUILDER`, `MULIB_IMPL_ROOTFS`, `MULIB_IMPL_VFS`,
`MULIB_IMPL_ARCHIVE`, `MULIB_IMPL_BUFFER`. Example: `examples/modules/rootfsbuilder.c`.
