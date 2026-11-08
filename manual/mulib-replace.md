# MuLIB replace

[Manuals](README.md) · [Library index](index.md)

Allocate a string with every nonoverlapping occurrence replaced. Performs left-to-right non-overlapping substitutions. Returns a newly allocated string that the caller must free. Empty search strings are refused.

## Contract and behavior

`replace_all()` performs left-to-right, non-overlapping literal matches
on NUL-terminated byte strings. Replacement text is not searched again. It
returns a new malloc-owned string even when there are no matches; release it
with `free()`. NULL arguments, an empty needle, size overflow, or allocation
failure return NULL. An empty replacement deletes matches. Input strings are
never modified; there is no regex, Unicode folding, or locale-aware matching.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_REPLACE
#include "mulib-replace.h"
int main(void) {
    char *s = mulib_replace_all("hello", "l", "!");
    assert(s && !strcmp(s, "he!!o"));
    free(s);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-replace.h`](../include/mulib-replace.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-replace.h
 * @brief Allocate a string with every nonoverlapping occurrence replaced.
 * @defgroup mulib_replace MuLIB replace
 * @{
 * @details Performs left-to-right non-overlapping substitutions. Returns a
 * newly allocated string that the caller must free. Empty search strings are
 * refused.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_REPLACE before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_REPLACE_H
#define MULIB_REPLACE_H
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

  /** @brief mulib replace all; see manual/mulib-replace.md for contracts and
   * examples. */
  char *mulib_replace_all (const char *source, const char *needle,
                           const char *replacement);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_REPLACE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-replace.h`](../include/mulib-replace.h) |
| Implementation switch | `MULIB_IMPL_REPLACE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/replace.c`](../examples/modules/replace.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/replace.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
