# MuLIB split

[Manuals](README.md) · [Library index](index.md)

In-place token splitting into an array of field pointers. Splits on a single byte delimiter and writes NUL terminators into the input. If output capacity is exceeded, the remainder is left unsplit and count equals capacity.

## Contract and behavior

`inplace()` writes NULs over delimiter bytes and stores pointers into
the caller's mutable string. Empty input produces one empty field; adjacent and
trailing delimiters preserve empty fields. Capacity is the number of pointer
slots. When it fills, scanning stops before replacing the next delimiter, so
the last returned field contains the unsplit remainder. Return is the number
of pointers written; invalid input/zero capacity returns zero. No allocation
occurs; pointers remain valid only while the original buffer does.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_SPLIT
#include "mulib-split.h"
int main(void) {
    char text[] = "a,b";
    char *fields[2];
    assert(mulib_split_inplace(text, ',', fields, 2) == 2);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-split.h`](../include/mulib-split.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-split.h
 * @brief In-place token splitting into an array of field pointers.
 * @defgroup mulib_split MuLIB split
 * @{
 * @details Splits on a single byte delimiter and writes NUL terminators into
 * the input. If output capacity is exceeded, the remainder is left unsplit and
 * count equals capacity.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_SPLIT before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_SPLIT_H
#define MULIB_SPLIT_H
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

  /** @brief mulib split inplace; see manual/mulib-split.md for contracts and
   * examples. */
  size_t mulib_split_inplace (char *text, char delimiter, char **fields,
                              size_t capacity);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SPLIT_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-split.h`](../include/mulib-split.h) |
| Implementation switch | `MULIB_IMPL_SPLIT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/split.c`](../examples/modules/split.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/split.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
