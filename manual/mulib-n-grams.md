# Byte n-grams

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_N_GRAMS`. [Example](../examples/modules/n-grams.c).

`mulib_ngrams` enumerates every overlapping width-byte window and its byte
offset. Windows borrow the source, are not NUL-terminated, and are not copied.
Callback nonzero continues; zero stops. Result counts callbacks including the
stopping call. Zero width, missing callback or NULL nonempty source returns
SIZE_MAX. Width greater than length yields zero calls. NULL empty source works
with positive width. No allocation; O(length) calls, constant storage.
Tokenization, frequency counting, boundary padding and Unicode segmentation
are deliberately separate concerns.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-n-grams.h`](../include/mulib-n-grams.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-n-grams.h
 * @brief Borrowed sliding byte n-gram enumeration.
 * Define MULIB_IMPL_N_GRAMS. No tokenization, padding or allocation.
 */
#ifndef MULIB_N_GRAMS_H
#define MULIB_N_GRAMS_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int (*mulib_ngram_visit_fn)(const unsigned char *gram, size_t length, size_t offset, void *context);
/** Visit length-width+1 windows, including overlaps. Nonzero callback continues.
 * Return callback count (including stopping call), SIZE_MAX for invalid/zero width. */
size_t mulib_ngrams(const void *bytes, size_t length, size_t width, mulib_ngram_visit_fn visit, void *context);
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
| Header | [`include/mulib-n-grams.h`](../include/mulib-n-grams.h) |
| Implementation switch | `MULIB_IMPL_N_GRAMS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/n-grams.c`](../examples/modules/n-grams.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/n-grams.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
