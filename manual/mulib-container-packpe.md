# PE container image packing

Include `mulib-container-packpe.h`; define `MULIB_IMPL_CONTAINER_PACKPE`,
`MULIB_IMPL_IMGFORMAT`, `MULIB_IMPL_CONTAINER`, `MULIB_IMPL_CRC32`,
`MULIB_IMPL_PE`, `MULIB_IMPL_COFF`, `MULIB_IMPL_ENDIAN`.
[Example](../examples/modules/container-packpe.c).

`object(image, length, machine, out, limit)` creates an AMD64 (`0x8664`) PE32+
data-only DLL profile with a read-only `.mulib` section. Despite the parallel
API name this is a **PE image, not a relocatable COFF object**. It has no entry
point, imports, base relocations, runtime, signature or execution promise.
Other machines return ENOTSUP. Headers are 512 bytes; raw payload is padded to
512 bytes, mapped at RVA 4096. Container extraction uses VirtualSize, not padding.

`append` preserves an existing PE32/PE32+ file and adds the image plus a
`MPEFPK1\0` trailer. Images with a nonempty certificate directory are rejected
with ENOTSUP rather than silently invalidating signing metadata. File checksums
are not updated; no Windows startup code is injected.

`open` validates a final trailer or unique `.mulib` section and the exact MuLIB
image envelope. A section whose virtual payload exceeds raw file size fails.
It returns borrowed bytes and a borrowed image view; no image returns ENOENT.
Writes atomically replace initialized blobs with alias-safe behavior. See
[shared limits, locator and error contracts](binary-formats.md). CRC is not
authentication; PE directory/signature contents are not interpreted.
