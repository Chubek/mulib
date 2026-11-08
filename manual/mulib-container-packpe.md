# PE container image packing

[Manuals](README.md) · [Library index](index.md)

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

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-container-packpe.h`](../include/mulib-container-packpe.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-container-packpe.h
 * @brief PE32+ data-image packing and non-destructive unsigned PE overlays.
 * Define MULIB_IMPL_CONTAINER_PACKPE plus IMGFORMAT/PE dependencies. */
#ifndef MULIB_CONTAINER_PACKPE_H
#define MULIB_CONTAINER_PACKPE_H
#include "mulib-imgformat.h"
#include "mulib-pe.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Construct AMD64 (machine 0x8664) PE32+ data-only DLL, no entry point,
 * imports or signature. Unlike COFF this is an image, not a relocatable object. */
int mulib_container_packpe_object (const void *image, size_t length, uint16_t machine, mulib_ct_blob *out, size_t limit);
/** @brief Append image/CRC trailer, preserving bytes. Reject certificate directories;
 * not a signing API and does not repair checksums or runtime metadata. */
int mulib_container_packpe_append (const void *file, size_t file_length, const void *image, size_t image_length, mulib_ct_blob *out, size_t limit);
/** @brief Borrow validated image from trailer or unique .mulib section, excluding padding. */
int mulib_container_packpe_open (const void *file, size_t length, size_t max_image, const unsigned char **image, size_t *image_length, mulib_imgformat_view *view);
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
| Header | [`include/mulib-container-packpe.h`](../include/mulib-container-packpe.h) |
| Implementation switch | `MULIB_IMPL_CONTAINER_PACKPE` |
| Repository-local includes | [mulib-imgformat](mulib-imgformat.md), [mulib-pe](mulib-pe.md) |
| Complete example | [`examples/modules/container-packpe.c`](../examples/modules/container-packpe.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/container-packpe.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
