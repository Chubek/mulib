# Mach-O container image packing

Include `mulib-container-packmacho.h`; define `MULIB_IMPL_CONTAINER_PACKMACHO`,
`MULIB_IMPL_IMGFORMAT`, `MULIB_IMPL_CONTAINER`, `MULIB_IMPL_CRC32`,
`MULIB_IMPL_MACHO`, `MULIB_IMPL_ENDIAN`.
[Example](../examples/modules/container-packmacho.c).

`object(image, length, cpu, out, limit)` creates a deterministic little-endian
Mach-O64 MH_OBJECT, with one `__DATA,__mulib_image` regular section. CPU is
x86-64 `0x01000007` or arm64 `0x0100000c`; other CPUs return ENOTSUP.
Headers occupy 192 bytes, payload alignment is eight bytes, and total length
must fit UINT32_MAX and the caller limit. No symbols, relocations or executable
entry are created; linking/section retention are the caller's responsibility.

`append` accepts a checked thin Mach-O file (32/64-bit, either endian), retaining
all original bytes and appending the image and `MMACPK1\0` trailer. Presence of
LC_CODE_SIGNATURE returns ENOTSUP; this API does not sign files or rewrite load
commands. `open` prefers a final valid locator, otherwise searches for exactly
one `__DATA,__mulib_image` section. Zero-fill sections and duplicates fail.

Writes atomically replace initialized blobs and allow input aliases. `open`
returns borrowed image bytes and view, unchanged on errors; missing image is
ENOENT. See [shared contracts](binary-formats.md) for size limits, checksums and
locator encoding. Fat files and authenticated/executable container stubs are not
supported. CRCs only detect accidental corruption.
