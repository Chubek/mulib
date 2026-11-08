# MuLIB levenshtein

[Manuals](README.md) · [Library index](index.md)

Levenshtein byte-string distance with two-row dynamic programming. Computes unit-cost insertion, deletion, and substitution distance between C strings. The algorithm compares bytes, not Unicode graphemes.

## Contract and behavior

Distance counts byte insertions, deletions, and substitutions at unit
cost between NUL-terminated strings. It does not decode Unicode or support
transposition. Empty-string distance is the other string's byte length. NULL
input, allocation failure, or size overflow returns `SIZE_MAX`; a successful
zero means equality. Runtime is O(n*m), using two temporary rows of O(m) values
where m is the second string's length. Inputs are borrowed only during the call.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_LEVENSHTEIN
#include "mulib-levenshtein.h"
int main(void) {
    assert(mulib_levenshtein_distance("cat", "cut") == 1);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-levenshtein.h`](../include/mulib-levenshtein.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-levenshtein.h
 * @brief Levenshtein byte-string distance with two-row dynamic programming.
 * @defgroup mulib_levenshtein MuLIB levenshtein
 * @{
 * @details Computes unit-cost insertion, deletion, and substitution distance
 * between C strings. The algorithm compares bytes, not Unicode graphemes.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_LEVENSHTEIN before
 * including this file. Other translation units may include it without the
 * implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_LEVENSHTEIN_H
#define MULIB_LEVENSHTEIN_H
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

  /** @brief mulib levenshtein distance; see manual/mulib-levenshtein.md for
   * contracts and examples. */
  size_t mulib_levenshtein_distance (const char *a, const char *b);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_LEVENSHTEIN_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-levenshtein.h`](../include/mulib-levenshtein.h) |
| Implementation switch | `MULIB_IMPL_LEVENSHTEIN` |
| Repository-local includes | None |
| Complete example | [`examples/modules/levenshtein.c`](../examples/modules/levenshtein.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/levenshtein.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
