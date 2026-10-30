# ocicontainervolume

Declarative bind-volume specification. `init` normalizes absolute source and
target paths into fixed-capacity owned arrays and validates readonly=0/1. Rejects
above-root paths, controls/backslashes and root target. Returns 1 success,
0 invalid/capacity error with output unchanged. No allocation or host operations.

Lexical validation is **not symlink confinement**. A privileged mount backend
must independently enforce native path resolution, race protection, permissions,
mount options and isolation. This library does not call mount/chroot.
Switches: `MULIB_IMPL_OCICONTAINERVOLUME`, `MULIB_IMPL_VFS`.
Example: `examples/modules/ocicontainervolume.c`.
