# mulib-container-vfs

## NAME AND INTEGRATION

Per-container path routing, read-only bindings and root checkpoints.
Define `MULIB_IMPL_CONTAINER_VFS` and the `rootfs` transitive switches. Example:
`examples/modules/container-vfs.c`.

## ROUTING AND OWNERSHIP

`init(filesystem,root,max_mounts,read_only)` borrows a live rootfs and mounts its
memory backend at `/`. max_mounts includes root. The object owns its VFS router,
binding pointer table, copied paths/operations and stable heap wrappers; backend
contexts remain borrowed. Use `filesystem.router` with the normal VFS read/write/
stat/list APIs or a FUSE adapter.

`bind(path,operations,context,read_only)` normalizes the mount path, copies operations
and installs a wrapper that enforces per-mount or whole-filesystem read-only.
Longest component-prefix routing follows VFS. Duplicate exact paths fail. Missing
read/stat/list callbacks produce ENOTSUP; absent/denied write produces EROFS.

`unbind(path)` removes an exact non-root mount. Nested mounts remain installed.
The wrapper is released but backend context is untouched. Successful bind/unbind
increments generation, with exhaustion checked before mutation. Treat fields as
read-only after initialization; changing read_only directly does not rewrite wrappers.

## CHECKPOINTS

`checkpoint(filesystem,out)` deep-copies only the root into an uninitialized
rootfs. Other mounted backends are intentionally outside the snapshot.
`restore(filesystem,checkpoint)` constructs a new root under the current root's
node/per-file limits, then commits it atomically. Root object address and mount
contexts remain valid. Bindings outside root are preserved. Whole-filesystem
read-only rejects restore. Restoration increments generation.

## RESULTS AND RELEASE

This module uses native VFS errors, not CT numeric codes. Allocation failures
leave bind/restore state unchanged. `free` unmounts and frees wrappers/table,
never root/backend data. Release FUSE sessions/daemon leases first and ensure no
callback is using the router. Keep root at a stable address while mounted.

Paths are lexical userspace paths. No symlink resolution, host mounting, kernel
permissions, inode/open-handle semantics or OS namespace changes are implied.
See [containerd-mount](mulib-containerd-mount.md) for export leases.
