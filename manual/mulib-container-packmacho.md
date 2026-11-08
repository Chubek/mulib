# Mach-O container image packing

[Manuals](README.md) · [Library index](index.md)

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

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-container-packmacho.h`](../include/mulib-container-packmacho.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-container-packmacho.h
 * @brief Mach-O64 image objects and non-destructive unsigned Mach-O overlays.
 * Define MULIB_IMPL_CONTAINER_PACKMACHO plus IMGFORMAT/MACHO dependencies. */
#ifndef MULIB_CONTAINER_PACKMACHO_H
#define MULIB_CONTAINER_PACKMACHO_H
#include "mulib-imgformat.h"
#include "mulib-macho.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Construct little-endian MH_OBJECT with __DATA,__mulib_image; native
 * x86-64 or arm64 CPU type, no symbols/relocations. Replace out atomically. */
int mulib_container_packmacho_object (const void *image, size_t length, uint32_t cpu, mulib_ct_blob *out, size_t limit);
/** @brief Preserve file bytes; reject LC_CODE_SIGNATURE. Appending is not signing. */
int mulib_container_packmacho_append (const void *file, size_t file_length, const void *image, size_t image_length, mulib_ct_blob *out, size_t limit);
/** @brief Borrow one validated image from trailer or __DATA,__mulib_image section. */
int mulib_container_packmacho_open (const void *file, size_t length, size_t max_image, const unsigned char **image, size_t *image_length, mulib_imgformat_view *view);
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
| Header | [`include/mulib-container-packmacho.h`](../include/mulib-container-packmacho.h) |
| Implementation switch | `MULIB_IMPL_CONTAINER_PACKMACHO` |
| Repository-local includes | [mulib-imgformat](mulib-imgformat.md), [mulib-macho](mulib-macho.md) |
| Complete example | [`examples/modules/container-packmacho.c`](../examples/modules/container-packmacho.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/container-packmacho.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
