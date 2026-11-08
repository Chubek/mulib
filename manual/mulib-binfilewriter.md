# binfilewriter

[Manuals](README.md) · [Library index](index.md)

Writes to an initialized shared byte buffer. `uint` appends unsigned LE integers
of width 1..8; `align` appends zero bytes to a power-of-two alignment; `patch`
changes an existing bounded field. All return 1 success, 0 invalid/overflow/OOM
and preserve logical output on failure. Values must fit requested width.
Alignment uses existing capacity directly and performs at most one realloc when
growth is needed; it does not allocate a separate padding buffer. Zero-fill and
length changes occur only after growth succeeds. Align/patch reject inconsistent
buffer length/capacity metadata.

No native format structs, external files or automatic finalization. Caller owns
the buffer and frees it with `mulib_buffer_free`.
Switches: `MULIB_IMPL_BINFILEWRITER`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_BYCUTILS`.
Example: `examples/modules/binfilewriter.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-binfilewriter.h`](../include/mulib-binfilewriter.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-binfilewriter.h
 * @brief Binary integer and zero-padding writer over MuLIB buffers.
 * Define MULIB_IMPL_BINFILEWRITER plus BUFFER and BYCUTILS switches. */
#ifndef MULIB_BINFILEWRITER_H
#define MULIB_BINFILEWRITER_H
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Append a 1..8 byte unsigned little-endian integer; 1 succeeds. */
  int mulib_binfilewriter_uint (mulib_buffer_t *out, unsigned width,
                                uint64_t value);
  /** @brief Append zero padding to power-of-two alignment, atomically. */
  int mulib_binfilewriter_align (mulib_buffer_t *out, size_t alignment);
  /** @brief Patch within existing length; 1 succeeds, otherwise unchanged. */
  int mulib_binfilewriter_patch (mulib_buffer_t *out, size_t offset,
                                 unsigned width, uint64_t value);
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
| Header | [`include/mulib-binfilewriter.h`](../include/mulib-binfilewriter.h) |
| Implementation switch | `MULIB_IMPL_BINFILEWRITER` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-bycutils](mulib-bycutils.md) |
| Complete example | [`examples/modules/binfilewriter.c`](../examples/modules/binfilewriter.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/binfilewriter.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
