# Byte edit distance

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_EDIT_DISTANCE` and `MULIB_IMPL_LEVENSHTEIN`.
[Example](../examples/modules/edit_distance.c).

The C-string API reuses MuLIB Levenshtein. The `_n` API compares binary buffers,
including embedded NUL, with unit insertion/deletion/substitution costs.
Matching costs zero; no transpositions or Unicode grapheme segmentation.
NULL buffers require zero length in `_n`; C-string inputs cannot be NULL.
SIZE_MAX signals invalid input, dimension overflow or allocation failure.
Arrays are borrowed. `_n` uses O(min(n,m)) temporary storage and O(n*m) time;
empty-side results require no allocation. Caller must bound computational work.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-edit_distance.h`](../include/mulib-edit_distance.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-edit_distance.h
 * @brief Unit-cost byte edit distance for binary buffers and C strings.
 * Define MULIB_IMPL_EDIT_DISTANCE and MULIB_IMPL_LEVENSHTEIN.
 */
#ifndef MULIB_EDIT_DISTANCE_H
#define MULIB_EDIT_DISTANCE_H
#include "mulib-levenshtein.h"
#ifdef __cplusplus
extern "C" {
#endif
/** C-string distance; SIZE_MAX on invalid input or allocation failure. */
size_t mulib_edit_distance(const char *a, const char *b);
/** Binary-buffer distance, O(n*m) time and O(min(n,m)) storage.
 * NULL is permitted for zero lengths. SIZE_MAX indicates failure. */
size_t mulib_edit_distance_n(const void *a, size_t n, const void *b, size_t m);
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
| Header | [`include/mulib-edit_distance.h`](../include/mulib-edit_distance.h) |
| Implementation switch | `MULIB_IMPL_EDIT_DISTANCE` |
| Repository-local includes | [mulib-levenshtein](mulib-levenshtein.md) |
| Complete example | [`examples/modules/edit_distance.c`](../examples/modules/edit_distance.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/edit_distance.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
