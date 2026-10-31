# rootfs

Bounded in-memory root filesystem sharing VFS storage. `init` sets node/per-file
limits; root is implicit. `import` constructs a fresh root from a complete USTAR
archive, creating missing parents, rejecting duplicate explicit paths and `.wh.*`
components. Only regular files/directories; archive parser rejects unsafe paths,
links/special files/PAX/GNU extensions/compression. Failure frees temporary state
and preserves output. Explicit-entry markers use one byte per materialized node;
archive records are parsed once. VFS path lookup remains linear in node count.

Returns 0 success and negative VFS errors. Archive syntax, unsafe paths and
whiteouts return EFORMAT; allocation and quota failures return ENOMEM and ELIMIT.
Duplicate explicit entries return EEXIST. `free` releases/reset all bytes.
Nothing is extracted onto host. No symlinks, ownership/permission enforcement,
OCI overlay whiteouts or mount isolation. Use VFS APIs for file operations.
Switches: `MULIB_IMPL_ROOTFS`, `MULIB_IMPL_VFS`, `MULIB_IMPL_ARCHIVE`,
`MULIB_IMPL_BUFFER`. Example: `examples/modules/rootfs.c`.

## Filesystem operations and checkpoints

`mulib_rootfs_clone` deep-copies into an uninitialized destination. Path and data
storage are independent. Source node/per-file limits are preserved. Allocation
failure releases temporary state and leaves output unchanged.

`mkdirs` creates missing parents, treats existing directories as success, and
rejects file parents. Its already-created parents remain on later failure.
`put` copies/replaces a whole file and optionally creates parents; the final file
replacement is atomic, while parent creation follows `mkdirs`. Paths use VFS
normalization. `read` returns an offset range and zeroes count before validation;
EOF is a successful zero-length read. `stat` includes the implicit root.

`remove` accepts files and empty directories. With recursive enabled it removes
descendants in reverse materialization order before the parent. Root cannot be
removed. `usage` returns aggregate regular-file bytes, file count and explicit
directory count, with overflow checking; output counters are optional.

`merge` applies a layer to a temporary clone and commits only on complete
success. Existing directories merge. Existing files require replace enabled;
file/directory type conflicts always fail. The destination's node/per-file quotas
remain authoritative. Layer paths/data may come from the destination itself.

## Aggregate import limits

`import_limited(tar, length, max_nodes, max_file_bytes, max_total_bytes, out)`
performs a validating archive preflight and checks aggregate payload bytes before
allocating filesystem storage. It then uses the same importer, including duplicate
and whiteout rejection. This avoids constructing an oversized filesystem before
discovering its total size. Archive input is borrowed and must remain immutable
through both passes.

## Transactional recipes

Use [rootfs-buildspec](mulib-rootfs-buildspec.md) to combine mkdir/write/copy/remove
operations with all-or-nothing publication. Use [container-vfs](mulib-container-vfs.md)
for per-container routing and read-only bindings, and
[imgbuilder](mulib-imgbuilder.md) for sorted deterministic USTAR output.

The rootfs storage layout is unchanged; imports normalize archive parser errors
to the VFS error family rather than exposing overlapping archive/VFS codes.
`tests/test_container_stack.c` covers cloning, recursive removal, transactional
failure and checkpoint restore. No method extracts paths onto the host.
