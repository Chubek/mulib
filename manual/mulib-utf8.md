# MuLIB utf8

[Manuals](README.md) · [Library index](index.md)

UTF-8 validation and single-code-point decoding. Rejects overlong encodings, surrogates, truncation, and scalar values above U+10FFFF. This module does not implement Unicode normalization, case folding, or grapheme segmentation.

## Contract and behavior

Decode reads exactly one Unicode scalar from the start of an explicit
byte span; it returns 1 on success and optionally reports consumed bytes.
Truncation, invalid continuation bytes, overlong encodings, surrogates, and
values above U+10FFFF return 0 without changing outputs. Encode returns 1..4
bytes written, or zero for invalid scalar/insufficient capacity; it adds no NUL.

Validate checks the entire span; `(NULL,0)` is valid empty input. Embedded U+0000
is a valid scalar. These operations allocate nothing and do not normalize text,
count graphemes, or interpret display width.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_UTF8
#include "mulib-utf8.h"
int main(void) {
    uint32_t cp = 0;
    assert(mulib_utf8_decode((const unsigned char *)"A", 1, &cp, NULL) &&
           cp == 65);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-utf8.h`](../include/mulib-utf8.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-utf8.h
 * @brief UTF-8 validation and single-code-point decoding.
 * @defgroup mulib_utf8 MuLIB utf8
 * @{
 * @details Rejects overlong encodings, surrogates, truncation, and scalar
 * values above U+10FFFF. This module does not implement Unicode normalization,
 * case folding, or grapheme segmentation.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_UTF8 before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_UTF8_H
#define MULIB_UTF8_H
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

  /** @brief mulib utf8 decode; see manual/mulib-utf8.md for contracts and
   * examples. */
  int mulib_utf8_decode (const unsigned char *bytes, size_t length,
                         uint32_t *codepoint, size_t *consumed);
  /** @brief mulib utf8 validate; see manual/mulib-utf8.md for contracts and
   * examples. */
  int mulib_utf8_validate (const unsigned char *bytes, size_t length);
  /** @brief mulib utf8 encode; see manual/mulib-utf8.md for contracts and
   * examples. */
  size_t mulib_utf8_encode (uint32_t codepoint, unsigned char *out,
                            size_t capacity);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_UTF8_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-utf8.h`](../include/mulib-utf8.h) |
| Implementation switch | `MULIB_IMPL_UTF8` |
| Repository-local includes | None |
| Complete example | [`examples/modules/utf8.c`](../examples/modules/utf8.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/utf8.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
