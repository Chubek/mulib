# binfilereader

[Manuals](README.md) · [Library index](index.md)

Initialize a cursor over borrowed bytes. `uint` reads unsigned little-endian
integers of width 1..8; `bytes` borrows/consumes a slice; `seek` permits absolute
positions through EOF. Return 1 success, 0 invalid/bounds, preserving cursor and
output on failure. Empty byte reads are allowed, integer reads require bytes.

Reader owns nothing and must not outlive input. No implicit endian guessing,
stream buffering, native casts or file I/O.
Switches: `MULIB_IMPL_BINFILEREADER`, `MULIB_IMPL_BINFILE`, `MULIB_IMPL_BYCUTILS`.
Example: `examples/modules/binfilereader.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-binfilereader.h`](../include/mulib-binfilereader.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-binfilereader.h
 * @brief Bounds-checked cursor over borrowed binary bytes.
 * Define MULIB_IMPL_BINFILEREADER plus BINFILE and BYCUTILS switches. */
#ifndef MULIB_BINFILEREADER_H
#define MULIB_BINFILEREADER_H
#include "mulib-binfile.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_binfilereader
  {
    mulib_binfile_t file;
    size_t position;
  } mulib_binfilereader_t;
  /** @brief Initialize cursor over borrowed input; 1 success. */
  int mulib_binfilereader_init (mulib_binfilereader_t *r, const void *data,
                                size_t size);
  /** @brief Read 1..8 little-endian bytes; cursor/output unchanged on failure.
   */
  int mulib_binfilereader_uint (mulib_binfilereader_t *r, unsigned width,
                                uint64_t *value);
  /** @brief Borrow and consume bytes; zero-size NULL span is valid. */
  int mulib_binfilereader_bytes (mulib_binfilereader_t *r, size_t size,
                                 mulib_binfile_t *out);
  /** @brief Absolute seek within [0,size]; 1 success. */
  int mulib_binfilereader_seek (mulib_binfilereader_t *r, size_t position);
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
| Header | [`include/mulib-binfilereader.h`](../include/mulib-binfilereader.h) |
| Implementation switch | `MULIB_IMPL_BINFILEREADER` |
| Repository-local includes | [mulib-binfile](mulib-binfile.md), [mulib-bycutils](mulib-bycutils.md) |
| Complete example | [`examples/modules/binfilereader.c`](../examples/modules/binfilereader.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/binfilereader.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
