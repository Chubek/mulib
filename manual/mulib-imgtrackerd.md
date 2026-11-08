# imgtrackerd

[Manuals](README.md) · [Library index](index.md)

In-memory reference counts keyed by syntactically valid OCI sha256 digests.
Zero initialize. `retain` increments with overflow checking; `release` decrements
or returns 1 when unretained; `count` returns zero absent/invalid. Changes return
0 success, negative invalid/allocation errors. Zero-count keys remain stored.
`free` releases/reset records. No image/blob ownership is transferred.

Not a daemon, persistent tracker, authenticated content store or blob garbage
collector. Syntax checks do not verify a digest against content.
Switches: `MULIB_IMPL_IMGTRACKERD`, `MULIB_IMPL_MEMTABLE`, `MULIB_IMPL_BYCUTILS`,
`MULIB_IMPL_OCIIMAGE`, `MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_STRBUF`,
`MULIB_IMPL_UTF8`. Example: `examples/modules/imgtrackerd.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-imgtrackerd.h`](../include/mulib-imgtrackerd.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-imgtrackerd.h
 * @brief Compatibility entry point for archived OCI image cache tracking.
 * Define MULIB_IMPL_IMGTRACKERD and dependencies once; see its manual.
 */
#ifndef MULIB_IMGTRACKERD_COMPAT_H
#define MULIB_IMGTRACKERD_COMPAT_H
#include "mulib-memtable.h"
#include "mulib-bycutils.h"
#include "mulib-ociimage.h"
#include "../.deprecated/mulib-imgtrackerd.h"
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-imgtrackerd.h`](../include/mulib-imgtrackerd.h) |
| Implementation switch | `MULIB_IMPL_IMGTRACKERD` |
| Repository-local includes | [`.deprecated/mulib-imgtrackerd.h`](../.deprecated/mulib-imgtrackerd.h), [mulib-bycutils](mulib-bycutils.md), [mulib-memtable](mulib-memtable.md), [mulib-ociimage](mulib-ociimage.md) |
| Complete example | [`examples/modules/imgtrackerd.c`](../examples/modules/imgtrackerd.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/imgtrackerd.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
