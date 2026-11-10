# MuLIB search

## Overlapping match enumeration

`mulib_search_all` uses byte KMP and calls `visitor(offset,user)` for every match,
including overlapping occurrences, in ascending offset order. A zero callback
return stops after counting that match. Return value is the number emitted, or
SIZE_MAX for invalid arguments/allocation failure. The callback is required;
NULL buffers are accepted only for zero corresponding lengths. An empty needle
emits exactly offset 0, even in an empty haystack. Callbacks must not modify the
borrowed input buffers. Time O(haystack + needle + callback work), scratch O(needle).

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
/* MuLIB compiler tools: Patched byte KMP search with callback-driven enumeration of overlapping occurrences.
 * ISO C11 single-header module. Define MULIB_IMPL_SEARCH in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_SEARCH_H
#define MULIB_SEARCH_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_search_kmp(const void *haystack,size_t hay_len,const void *needle,size_t needle_len);
typedef int (*mulib_search_match_fn)(size_t offset,void *user);
/* Counts emitted matches; SIZE_MAX invalid or OOM. Empty needle emits offset 0. */
size_t mulib_search_all(const void *haystack,size_t hay_len,const void *needle,size_t needle_len,mulib_search_match_fn visitor,void *user);
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
