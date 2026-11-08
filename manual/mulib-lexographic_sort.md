# Lexicographic byte ordering

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_LEXOGRAPHIC_SORT`.
[Example](../examples/modules/lexographic_sort.c). Filename spelling retained.

Compare NUL-terminated byte strings as unsigned bytes, optionally folding ASCII
A-Z to a-z. Comparator returns negative/zero/positive; NULL sorts before strings.
Sort rejects NULL entries (or missing nonempty array) before changing the array.
It stably rearranges caller-owned pointers, never copies or frees strings.
Empty arrays may be NULL. Insertion sort takes O(n²) comparisons and constant
storage. This is locale-independent byte ordering, not Unicode collation or
natural numeric sorting; UTF-8 bytes are not decoded.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-lexographic_sort.h`](../include/mulib-lexographic_sort.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-lexographic_sort.h
 * @brief Locale-independent unsigned-byte lexicographic string sorting.
 * Define MULIB_IMPL_LEXOGRAPHIC_SORT. Historical filename retained.
 */
#ifndef MULIB_LEXOGRAPHIC_SORT_H
#define MULIB_LEXOGRAPHIC_SORT_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Compare C strings, optionally folding ASCII A-Z; NULL sorts before non-NULL. */
int mulib_lexographic_compare(const char *a, const char *b, int fold_ascii);
/** Stable in-place sort of borrowed pointers. Return 0 on NULL entries/invalid
 * array, before mutation. Quadratic time, no allocations. */
int mulib_lexographic_sort(const char **strings, size_t n, int fold_ascii);
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
| Header | [`include/mulib-lexographic_sort.h`](../include/mulib-lexographic_sort.h) |
| Implementation switch | `MULIB_IMPL_LEXOGRAPHIC_SORT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/lexographic_sort.c`](../examples/modules/lexographic_sort.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/lexographic_sort.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
