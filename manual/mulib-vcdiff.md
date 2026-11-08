# vcdiff — RFC 3284 deltas

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_VCDIFF`; no other implementation switches are required.
See [`examples/modules/vcdiff.c`](../examples/modules/vcdiff.c).

`decode` implements the RFC 3284 default 256-entry code table, including paired
instructions, SELF/HERE/NEAR/SAME addressing, four near-cache and 768 same-cache
entries, overlapping target COPY, source windows, previous-target windows, and
multiple windows. Integer parsing is overflow-checked base-128 big-endian
VCDIFF encoding, not LEB128. All section sizes and consumed bytes must agree.
Source COPY cannot cross the source-segment boundary. COPY from target must
start in already produced data. Output capacity bounds decompression.

The decoder returns 1 success, 0 malformed, -1 unsupported feature, or -2
insufficient capacity. `used` is zero on failure, but output may contain a
partial result. Header extensions, custom tables, secondary compression,
nonzero version bytes and nonstandard checksums are explicitly unsupported.
A header-only delta is a valid empty target.

`encode` emits one standards-compatible window: a common-prefix SELF COPY
when at least four bytes match, followed by ADD for the remainder. It is a
simple differencer, not an optimal compressor. It returns 1/0 and preserves
output on insufficient capacity. `target_length + 128` is a sufficient bound
when representable. Both functions borrow all storage, allocate nothing, and
require disjoint output/source/delta or output/source/target buffers.

Reference: [RFC 3284](https://www.rfc-editor.org/rfc/rfc3284).
Tests: `tests/test_vcs_formats.c`, including a hand-encoded RFC example.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-vcdiff.h`](../include/mulib-vcdiff.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-vcdiff.h
 * @brief RFC 3284 default-table VCDIFF decoding and prefix-delta encoding.
 * Define MULIB_IMPL_VCDIFF once. No secondary compression, custom code tables,
 * application headers or nonstandard checksums; these return unsupported.
 */
#ifndef MULIB_VCDIFF_H
#define MULIB_VCDIFF_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Decode all windows using source and earlier target segments, all
   * default-table instructions/address modes. 1 success, 0 malformed, -1
   * unsupported, -2 capacity. used=0 on failure, output may be partially
   * written. Output must not overlap source or delta; all storage is
   * caller-owned. */
  int mulib_vcdiff_decode (const void *source, size_t source_length,
                           const void *delta, size_t delta_length,
                           void *output, size_t capacity, size_t *used);
  /** @brief Encode one window with common-prefix COPY and remaining ADD.
   * 1 success, 0 invalid/insufficient capacity; output unchanged on failure.
   * At most target_length+128 bytes required (if that sum is representable).
   */
  int mulib_vcdiff_encode (const void *source, size_t source_length,
                           const void *target, size_t target_length,
                           void *output, size_t capacity, size_t *used);
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
| Header | [`include/mulib-vcdiff.h`](../include/mulib-vcdiff.h) |
| Implementation switch | `MULIB_IMPL_VCDIFF` |
| Repository-local includes | None |
| Complete example | [`examples/modules/vcdiff.c`](../examples/modules/vcdiff.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/vcdiff.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
