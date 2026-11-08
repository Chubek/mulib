# ociimagebuilder

[Manuals](README.md) · [Library index](index.md)

`descriptor` creates owned JSON from mediaType, externally computed sha256 digest
and size <=2^53-1. `create` validates/copies config into a manifest with empty
layers. `layer` validates/copies a descriptor into an existing valid manifest,
up to 4096 layers. Inputs remain caller-owned. Constructors return NULL on
invalid/OOM; layer returns 1 success, 0 error without changing manifest.

Use shared JSON stringify/destroy APIs. No blob hashing, file reads, compression,
registry upload, authentication or signature generation.
Switches: `MULIB_IMPL_OCIIMAGEBUILDER`, `MULIB_IMPL_OCIIMAGE`, `MULIB_IMPL_JSON`,
`MULIB_IMPL_PARSEC`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_UTF8`.
Example: `examples/modules/ociimagebuilder.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ociimagebuilder.h`](../include/mulib-ociimagebuilder.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-ociimagebuilder.h
 * @brief Compatibility entry point for archived OCI manifest construction.
 * Define MULIB_IMPL_OCIIMAGEBUILDER and dependencies once; see its manual.
 */
#ifndef MULIB_OCIIMAGEBUILDER_COMPAT_H
#define MULIB_OCIIMAGEBUILDER_COMPAT_H
#include "mulib-ociimage.h"
#include "../.deprecated/mulib-ociimagebuilder.h"
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ociimagebuilder.h`](../include/mulib-ociimagebuilder.h) |
| Implementation switch | `MULIB_IMPL_OCIIMAGEBUILDER` |
| Repository-local includes | [`.deprecated/mulib-ociimagebuilder.h`](../.deprecated/mulib-ociimagebuilder.h), [mulib-ociimage](mulib-ociimage.md) |
| Complete example | [`examples/modules/ociimagebuilder.c`](../examples/modules/ociimagebuilder.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ociimagebuilder.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
