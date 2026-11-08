# bycutils

[Manuals](README.md) · [Library index](index.md)

Allocation-free binary-code arithmetic. `range` checks offset/length using
subtraction. `align` rounds size_t upward to a nonzero power of two, rejecting
overflow. `load`/`store` access unaligned little-endian integers of width 1..8.
Store rejects values that do not fit; all failures preserve outputs/bytes.
Functions return 1 success/valid, 0 invalid. Callers supply actual accessible
buffer sizes; no native structure casts or signed integer encoding.

Switch: `MULIB_IMPL_BYCUTILS`. Example: `examples/modules/bycutils.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-bycutils.h`](../include/mulib-bycutils.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-bycutils.h
 * @brief Checked byte-code arithmetic and unaligned little-endian integers.
 * Define MULIB_IMPL_BYCUTILS once. No native structure casts or allocation.
 */
#ifndef MULIB_BYCUTILS_H
#define MULIB_BYCUTILS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Check a range without overflowing offset+length. */
  int mulib_bycutils_range (size_t size, size_t offset, size_t length);
  /** @brief Round up to a nonzero power-of-two alignment; unchanged on
   * failure. */
  int mulib_bycutils_align (size_t value, size_t alignment, size_t *out);
  /** @brief Read 1..8 little-endian bytes; unchanged on invalid range/width.
   */
  int mulib_bycutils_load (const void *data, size_t size, size_t offset,
                           unsigned width, uint64_t *out);
  /** @brief Store 1..8 bytes; reject a value that does not fit, without
   * mutation. */
  int mulib_bycutils_store (void *data, size_t size, size_t offset,
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
| Header | [`include/mulib-bycutils.h`](../include/mulib-bycutils.h) |
| Implementation switch | `MULIB_IMPL_BYCUTILS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/bycutils.c`](../examples/modules/bycutils.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bycutils.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
