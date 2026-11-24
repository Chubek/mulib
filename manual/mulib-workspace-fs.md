# `mulib-workspace-fs.h`

**Macro:** `MULIB_IMPL_WORKSPACE_FS` in one C11 TU. Linux/POSIX only.
Compile with `_POSIX_C_SOURCE=200809L` and `_GNU_SOURCE` on Linux.

## Responsibilities

`mulib_wfs_open(&ws, root)` opens a host-approved root directory descriptor.
Supply a trusted root pathname; this API does not authorize the spelling or
walk its ancestor directories. Do not substitute a caller-supplied root.
`mulib_wfs_parent()` descends from that descriptor using only `openat()` with
`O_DIRECTORY|O_NOFOLLOW`; the returned basename is malloc-owned and the parent
fd is caller-owned. Every directory component remains anchored by its opened
fd. Paths cannot be absolute, contain a backslash, empty component, `.` or
`..`; the maximum path length is 4096 bytes. Names beginning `.` are otherwise
legal. No symlink following at any depth.

- `mulib_wfs_read` returns malloc-owned data and one spare NUL sentinel byte;
  the true binary length is always explicit. Only regular files are accepted.
- `mulib_wfs_stat` returns regular-file size, mode, device, inode.
- `mulib_wfs_write` stages same-directory data under an unpredictable
  `.mulib-wfs-...` name, flushes the file, conditionally installs by hard-link
  (create-only) or rename (replacement), then syncs its parent directory.
  Existing file permissions preserve ordinary `rwx` bits, not setuid/setgid.
- `mulib_wfs_remove` unlinks regular files and syncs the parent.
- `mulib_wfs_blob_free`, `mulib_wfs_close` release resources.

## Status codes

Zero means success; negative `MULIB_WFS_*` values include invalid, system,
permission/path denial, not-found, exists, limit, allocation failure, not
regular, stale/conflicting state and uncertain commit.

`UNCERTAIN` is intentional: if a parent-directory `fsync` fails after rename,
there may already be a visible change. Do not retry blindly. The caller must
re-read/compare state and reconcile.

## Security limits

These routines are **not** an entire sandbox and cannot prevent bind mounts,
pre-existing external hardlinks or concurrent writers outside a shared lock.
They do not enforce a quota on output writes, so the host must set budgets.
Do not use lexical path normalization alone for authorization.

The kernel does not create directories, execute programs, or interpret text as
shell commands. For multi-file operations use `mulib-workspace-edit.h`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-workspace-fs.h`](../kernels/mulib-workspace-fs.h) |
| Implementation switch | `MULIB_IMPL_WORKSPACE_FS` |
| Repository-local includes | None |
| Complete example | [`examples/apply_unidiff.c`](../examples/apply_unidiff.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/apply_unidiff.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
