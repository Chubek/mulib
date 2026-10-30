# rootfs

Bounded in-memory root filesystem sharing VFS storage. `init` sets node/per-file
limits; root is implicit. `import` constructs a fresh root from a complete USTAR
archive, creating missing parents, rejecting duplicate explicit paths and `.wh.*`
components. Only regular files/directories; archive parser rejects unsafe paths,
links/special files/PAX/GNU extensions/compression. Failure frees temporary state
and preserves output. Explicit-entry markers use one byte per materialized node;
archive records are parsed once. VFS path lookup remains linear in node count.

Returns 0 success, negative archive/VFS errors. `free` releases/reset all bytes.
Nothing is extracted onto host. No symlinks, ownership/permission enforcement,
OCI overlay whiteouts or mount isolation. Use VFS APIs for file operations.
Switches: `MULIB_IMPL_ROOTFS`, `MULIB_IMPL_VFS`, `MULIB_IMPL_ARCHIVE`,
`MULIB_IMPL_BUFFER`. Example: `examples/modules/rootfs.c`.
