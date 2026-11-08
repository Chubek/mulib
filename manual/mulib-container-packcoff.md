# COFF container image packing

[Manuals](README.md) · [Library index](index.md)

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

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-container-packcoff.h`](../include/mulib-container-packcoff.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-container-packcoff.h
 * @brief COFF image objects and checksummed non-destructive file overlays.
 * Define MULIB_IMPL_CONTAINER_PACKCOFF plus IMGFORMAT/COFF dependencies. */
#ifndef MULIB_CONTAINER_PACKCOFF_H
#define MULIB_CONTAINER_PACKCOFF_H
#include "mulib-imgformat.h"
#include "mulib-coff.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Replace initialized blob with one .mulib data section; native COFF machine. */
int mulib_container_packcoff_object (const void *image, size_t length, uint16_t machine, mulib_ct_blob *out, size_t limit);
/** @brief Preserve original bytes and append image plus CRC locator atomically. */
int mulib_container_packcoff_append (const void *file, size_t file_length, const void *image, size_t image_length, mulib_ct_blob *out, size_t limit);
/** @brief Borrow validated image from final trailer or unique .mulib section.
 * All outputs unchanged on error; CRC is not authentication. */
int mulib_container_packcoff_open (const void *file, size_t length, size_t max_image, const unsigned char **image, size_t *image_length, mulib_imgformat_view *view);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-container-packcoff.h`](../include/mulib-container-packcoff.h) |
| Implementation switch | `MULIB_IMPL_CONTAINER_PACKCOFF` |
| Repository-local includes | [mulib-coff](mulib-coff.md), [mulib-imgformat](mulib-imgformat.md) |
| Complete example | [`examples/modules/container-packcoff.c`](../examples/modules/container-packcoff.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/container-packcoff.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
