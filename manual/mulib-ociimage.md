# ociimage

[Manuals](README.md) · [Library index](index.md)

Focused OCI image-manifest JSON profile. `digest` requires `sha256:` plus exactly
64 lowercase hex digits. `descriptor` requires nonempty mediaType, that digest
and integral size 0..2^53-1. `validate` requires schemaVersion 2, OCI manifest
mediaType, config descriptor and <=4096 layer descriptors. Unknown extension
members remain in the shared JSON tree. Validators return 1 valid, 0 invalid.

`parse` returns an owned validated JSON tree or NULL with optional diagnostic;
destroy with `mulib_json_destroy`. No downloads, hash/content-size verification,
signatures, platform config checks, indexes, Docker media-type compatibility or
layer decompression. Syntax validation does **not authenticate image content**.
Switches: `MULIB_IMPL_OCIIMAGE`, `MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`,
`MULIB_IMPL_STRBUF`, `MULIB_IMPL_UTF8`. Example: `examples/modules/ociimage.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ociimage.h`](../include/mulib-ociimage.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-ociimage.h
 * @brief Compatibility entry point for archived OCI manifest validation.
 * Define MULIB_IMPL_OCIIMAGE and dependencies once; see its manual.
 */
#ifndef MULIB_OCIIMAGE_COMPAT_H
#define MULIB_OCIIMAGE_COMPAT_H
#include "mulib-json.h"
#include "../.deprecated/mulib-ociimage.h"
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ociimage.h`](../include/mulib-ociimage.h) |
| Implementation switch | `MULIB_IMPL_OCIIMAGE` |
| Repository-local includes | [`.deprecated/mulib-ociimage.h`](../.deprecated/mulib-ociimage.h), [mulib-json](mulib-json.md) |
| Complete example | [`examples/modules/ociimage.c`](../examples/modules/ociimage.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ociimage.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
