# Weighted edit distance

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_WEIGHED_EDIT_DISTANE`; link `-lm`.
[Example](../examples/modules/weighed_edit_distane.c). Filename spelling retained.

`mulib_weighted_edit_distance` transforms a binary buffer a into b, with supplied
finite nonnegative insertion, deletion and substitution costs. Equal bytes
cost zero. Direction matters if insertion and deletion differ. Zero costs are
valid. No transposition, context weighting or Unicode decoding is performed.

NULL buffers require zero length. Invalid input, dimension overflow and OOM
give NAN; numeric cost overflow gives INFINITY. Inputs are borrowed. Temporary
storage O(m), time O(n*m); no operation-count/work limit is imposed.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-weighed_edit_distane.h`](../include/mulib-weighed_edit_distane.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-weighed_edit_distane.h
 * @brief Weighted byte edit distance with directional insertion/deletion costs.
 * Define MULIB_IMPL_WEIGHED_EDIT_DISTANE; link libm. Filename retained.
 */
#ifndef MULIB_WEIGHED_EDIT_DISTANE_H
#define MULIB_WEIGHED_EDIT_DISTANE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Transform a into b, with finite nonnegative costs. Matching costs zero.
 * NULL buffers require zero length. NAN on invalid/OOM; INFINITY on overflow.
 * O(n*m) time, O(m) storage. No transpositions or Unicode segmentation. */
double mulib_weighted_edit_distance(const void *a, size_t n, const void *b, size_t m,
                                   double insertion, double deletion, double substitution);
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
| Header | [`include/mulib-weighed_edit_distane.h`](../include/mulib-weighed_edit_distane.h) |
| Implementation switch | `MULIB_IMPL_WEIGHED_EDIT_DISTANE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/weighed_edit_distane.c`](../examples/modules/weighed_edit_distane.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/weighed_edit_distane.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
