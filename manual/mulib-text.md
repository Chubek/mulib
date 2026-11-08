# MuLIB text

[Manuals](README.md) · [Library index](index.md)

In-place whitespace trimming and ASCII case conversion for mutable
NUL-terminated strings.

## Contract and behavior

Trim modifies trailing whitespace to NUL and returns a pointer past
leading whitespace inside the original mutable buffer. Retain the original
allocation pointer for freeing; do not free the returned interior pointer.
Whitespace classification uses C `isspace` and the current locale. ASCII lower/
upper modify only A..Z/a..z bytes; they do not perform Unicode case conversion.
All operate on NUL-terminated strings and accept NULL (trim returns NULL,
case conversion does nothing). No allocations occur.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_TEXT
#include "mulib-text.h"
int main(void) {
    char str[] = " ABC ";
    assert(!strcmp(mulib_text_trim(str), "ABC"));
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-text.h`](../include/mulib-text.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-text.h
 * @brief Locale-sensitive whitespace trim and ASCII case conversion routines.
 * @defgroup mulib_text MuLIB text
 * @{
 * @details Works on mutable NUL-terminated strings. The trim result points
 * inside the original buffer and must not be freed separately.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_TEXT before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_TEXT_H
#define MULIB_TEXT_H
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

  /** @brief mulib text trim; see manual/mulib-text.md for contracts and
   * examples. */
  char *mulib_text_trim (char *text);
  /** @brief mulib text ascii lower; see manual/mulib-text.md for contracts and
   * examples. */
  void mulib_text_ascii_lower (char *text);
  /** @brief mulib text ascii upper; see manual/mulib-text.md for contracts and
   * examples. */
  void mulib_text_ascii_upper (char *text);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_TEXT_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-text.h`](../include/mulib-text.h) |
| Implementation switch | `MULIB_IMPL_TEXT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/text.c`](../examples/modules/text.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/text.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
