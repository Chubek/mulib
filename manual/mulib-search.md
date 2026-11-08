# MuLIB search

[Manuals](README.md) · [Library index](index.md)

Substring finding via Knuth-Morris-Pratt with explicit byte lengths. Returns SIZE_MAX when no match is found or allocation fails, and 0 for an empty needle. Runs in O(haystack length + needle length) time.

## Contract and behavior

KMP searches explicit binary byte ranges, including embedded NULs,
and returns the zero-based first match offset. An empty needle returns zero,
even before pointer validation. `SIZE_MAX` means no match, invalid arguments,
size overflow, or allocation failure; these outcomes are not distinguished.
The prefix table uses O(needle length) temporary storage and is freed before
return. Runtime is O(haystack length + needle length). Inputs are never retained.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_SEARCH
#include "mulib-search.h"
int main(void) {
    assert(mulib_search_kmp("hello", 5, "ll", 2) == 2);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-search.h`](../include/mulib-search.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-search.h
 * @brief Substring finding via Knuth-Morris-Pratt with explicit byte lengths.
 * @defgroup mulib_search MuLIB search
 * @{
 * @details Returns SIZE_MAX when no match is found or allocation fails, and 0
 * for an empty needle. Runs in O(haystack length + needle length) time.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_SEARCH before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_SEARCH_H
#define MULIB_SEARCH_H
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief mulib search kmp; see manual/mulib-search.md for contracts and
   * examples. */
  size_t mulib_search_kmp (const void *haystack, size_t hay_len,
                           const void *needle, size_t needle_len);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SEARCH_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-search.h`](../include/mulib-search.h) |
| Implementation switch | `MULIB_IMPL_SEARCH` |
| Repository-local includes | None |
| Complete example | [`examples/modules/search.c`](../examples/modules/search.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/search.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
