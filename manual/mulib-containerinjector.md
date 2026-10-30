# containerinjector

`put` copies/replaces file content in an initialized memory rootfs, requiring
existing parent directories. Paths use VFS lexical absolute normalization;
configured node/per-file limits apply. Returns 0 success, VFS error otherwise.
Input may alias existing file storage because VFS copies before replacement.

This is data insertion into memory storage, **not process/code injection**, native
filesystem extraction, host access or permission bypass. Root owns copied bytes.
Switches: `MULIB_IMPL_CONTAINERINJECTOR`, `MULIB_IMPL_VFS`, `MULIB_IMPL_ROOTFS`,
`MULIB_IMPL_ARCHIVE`, `MULIB_IMPL_BUFFER`.
Example: `examples/modules/containerinjector.c`.
