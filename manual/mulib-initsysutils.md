# mulib-initsysutils

Define `MULIB_IMPL_INITSYSUTILS` and `MULIB_IMPL_CPIO` once.
[Example](../examples/modules/initsysutils.c). No dynamic allocation or implicit
process/filesystem operations.

## Linux initramfs

`next` iterates **uncompressed** newc/crc Linux initramfs buffers, accepting
concatenated archives, zero padding, an empty buffer and an omitted final
trailer. Headers must start at four-byte offsets. Compressed/unknown members
return UNSUPPORTED; decompress externally before inspection. Initialize cursor
with `{0}`. Entries borrow immutable input; negative errors leave cursor/output
unchanged. END=1 commits the final cursor. A trailer increments `cursor.archive`,
so consumers can reset hardlink identity at archive boundaries.

`add` checks paths and Linux file-type/data rules before using the shared CPIO
writer. Regular files and symlinks can carry data; other types require no data.
Symlink contents are a nonempty unterminated target, not a NUL-terminated string.
Link counts must be nonzero. Linux's 4096-byte filename storage and symlink
payload limits are enforced; oversized padded input names are rejected.
Caller controls IDs, permissions, timestamps, hardlinks, device nodes and parent
ordering. Call `mulib_cpio_finish` explicitly after the last entry.

`path` is a separate, deliberately restrictive extraction policy: root-relative
names, no `..`, empty components or embedded NUL; leading `./` and `.` are
allowed. Absolute and trailing-slash names are rejected even though generic CPIO
can represent them. A lexical check is not protection against filesystem symlink
traversal; no extraction is performed by this module.

`validate` scans all entries and returns count plus whether the last named
`init`/`./init` entry is a regular executable. This flag does not resolve
hardlinks/symlinks or prove interpreters, libraries or architecture are present.
Results are atomic on failure. Build a real `/init` and all its dependencies
before booting; the example demonstrates archive construction only.

## Initialization sequencing

`run` executes an explicit array of apply/undo callbacks. Every callback is
validated before work starts. A failed apply cleans its own partial resources;
completed steps are undone in reverse order, continuing after undo errors.
Return value is the original apply error; outputs identify the failed index and
first rollback error. On success failed_index is SIZE_MAX. A rollback failure
means the caller must reconcile remaining resources. No global registry is kept.
Use callbacks for mounts, devices or handoff policy, and `mulib-initsys.h` for
dependency-based service supervision or `mulib-initproc.h` for child reaping.
This module does not become PID 1 or implement switch_root/pivot_root.

Errors include shared CPIO errors and INVALID=-10, UNSUPPORTED=-11, PATH=-12.
All APIs are synchronous and require caller serialization for mutable state.
Reference: [Linux buffer grammar](https://docs.kernel.org/driver-api/early-userspace/buffer-format.html),
cross-checked with Linux `init/initramfs.c`, accessed 2026-11-06. Tests cover
concatenation, missing trailers, invalid paths and rollback failure ordering.
