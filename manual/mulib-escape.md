# MuLIB escape

[Manuals](README.md) · [Library index](index.md)

C-style escaping and unescaping of byte strings, supporting newline, tab,
carriage return, backslash, and double quote. Hex and octal escapes are unsupported.

## Contract and behavior

The supported C-style escapes are newline, tab, carriage return,
backslash, and double quote. Other bytes pass through unchanged during encoding;
this is not a full C string-literal serializer. Decoding rejects other escape
letters and a trailing backslash. Both functions take NUL-terminated input and
capacity including the output terminator.

`escape_c()` returns the encoded length excluding NUL, or zero on failure;
empty success also returns zero. `escape_unc()` returns 1/0. Capacity or syntax
failure may leave a partial, unterminated output. No allocations occur.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_ESCAPE
#include "mulib-escape.h"
int main(void) {
    char out[32];
    assert(mulib_escape_c("A\n", out, sizeof out) == 3);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-escape.h`](../include/mulib-escape.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-escape.h
 * @brief C-style escaping and unescaping of byte strings.
 * @defgroup mulib_escape MuLIB escape
 * @{
 * @details Escapes newline, tab, carriage return, backslash
 * and double quotes. This is not a full C lexical escape parser; hex and octal
 * escapes are unsupported.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_ESCAPE before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_ESCAPE_H
#define MULIB_ESCAPE_H
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

  /** @brief mulib escape c; see manual/mulib-escape.md for contracts and
   * examples. */
  size_t mulib_escape_c (const char *source, char *out, size_t capacity);
  /** @brief mulib escape unc; see manual/mulib-escape.md for contracts and
   * examples. */
  int mulib_escape_unc (const char *source, char *out, size_t capacity);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ESCAPE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-escape.h`](../include/mulib-escape.h) |
| Implementation switch | `MULIB_IMPL_ESCAPE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/escape.c`](../examples/modules/escape.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/escape.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
