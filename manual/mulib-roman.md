# MuLIB roman

[Manuals](README.md) · [Library index](index.md)

Roman numeral formatting and a simple validity-checked parser. Supports canonical Roman numerals 1..3999. Parser validates canonical spelling by formatting the computed value and comparing strings.

## Contract and behavior

Format accepts integers 1..3999 and writes canonical uppercase Roman
numerals with subtractive pairs. Capacity includes NUL; a 16-byte buffer covers
this range. It returns 1 on success, 0 on invalid input or insufficient space;
capacity failure may leave partial, unterminated output.

Parse accepts only the exact uppercase canonical spelling, verified by
reformatting. Whitespace, lowercase, and noncanonical repetitions are rejected.
It returns the numeric value or zero for invalid input. No allocation occurs.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_ROMAN
#include "mulib-roman.h"
int main(void) {
    char b[32];
    assert(mulib_roman_format(12, b, sizeof b));
    assert(!strcmp(b, "XII"));
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-roman.h`](../include/mulib-roman.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-roman.h
 * @brief Roman numeral formatting and a simple validity-checked parser.
 * @defgroup mulib_roman MuLIB roman
 * @{
 * @details Supports canonical Roman numerals 1..3999. Parser validates
 * canonical spelling by formatting the computed value and comparing strings.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_ROMAN before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_ROMAN_H
#define MULIB_ROMAN_H
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

  /** @brief mulib roman format; see manual/mulib-roman.md for contracts and
   * examples. */
  int mulib_roman_format (unsigned value, char *out, size_t cap);
  /** @brief mulib roman parse; see manual/mulib-roman.md for contracts and
   * examples. */
  unsigned mulib_roman_parse (const char *text);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ROMAN_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-roman.h`](../include/mulib-roman.h) |
| Implementation switch | `MULIB_IMPL_ROMAN` |
| Repository-local includes | None |
| Complete example | [`examples/modules/roman.c`](../examples/modules/roman.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/roman.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
