# COFF container image packing

Include `mulib-container-packcoff.h`; define `MULIB_IMPL_CONTAINER_PACKCOFF`,
`MULIB_IMPL_IMGFORMAT`, `MULIB_IMPL_CONTAINER`, `MULIB_IMPL_CRC32`,
`MULIB_IMPL_COFF`, `MULIB_IMPL_ENDIAN`.
[Example](../examples/modules/container-packcoff.c).

`object(image, length, machine, out, limit)` validates the existing MuLIB image
envelope and writes a deterministic COFF object with a read-only `.mulib` section.
Machine is native COFF (AMD64 `0x8664`, not ELF 62). Headers take 60 bytes;
total length must fit both the caller limit and UINT32_MAX. There are no symbols
or relocations. Linkers can discard an unreferenced section; this is not a startup
stub or a guarantee of section retention.

`append(file, file_length, image, image_length, out, limit)` preserves all original
bytes and appends image bytes plus a `MCOFPK1\0` locator. `open` prefers that final
trailer, otherwise finds one `.mulib` section, rejecting duplicates and BSS.
It validates bounds, both locator CRCs and the exact image envelope. Output spans
borrow the file. Missing image returns `MULIB_CT_ENOENT`.

All writes replace initialized blobs atomically, permit input/output aliasing,
and return container result codes. Limits and locator layout are specified in
[shared contracts](binary-formats.md). CRCs do not authenticate images. No archive,
bigobj, linking, relocation or execution is provided.
