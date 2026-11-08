# MuLIB base64

[Manuals](README.md) · [Library index](index.md)

RFC 4648 padded Base64 encoder and strict decoder using caller-owned buffers.

## Contract and behavior

Encoding uses the standard `+/` alphabet and `=` padding. Provide
`4 * ceil(length / 3) + 1` output bytes, with checked arithmetic in your caller.
The encoder returns the encoded length excluding its NUL terminator; zero also
represents a successful empty encoding. It validates capacity before writing.

Decoding requires a multiple-of-four input length, correct final padding, and
zero unused padding bits. Whitespace and URL-safe `-_` are rejected. It returns
1 on success and 0 on malformed input or insufficient capacity, leaving output
and optional `written` unchanged on failure. Decoded bytes have no added NUL.
No allocations occur; use separate input/output storage.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_BASE64
#include "mulib-base64.h"
int main(void) {
    char s[16];
    assert(mulib_base64_encode("Hi", 2, s, sizeof s) == 4);
    assert(!strcmp(s, "SGk="));
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-base64.h`](../include/mulib-base64.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-base64.h
 * @brief RFC 4648 padded Base64 encoder and strict decoder.
 * @defgroup mulib_base64 MuLIB base64
 * @{
 * @details Encoder requires a destination buffer including one final NUL.
 * Decoder accepts padded standard alphabet only (not URL-safe alphabet or
 * whitespace); validation and capacity failures leave output and the optional
 * written count unchanged.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_BASE64 before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_BASE64_H
#define MULIB_BASE64_H
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

  /** @brief mulib base64 encode; see manual/mulib-base64.md for contracts and
   * examples. */
  size_t mulib_base64_encode (const void *data, size_t n, char *out,
                              size_t capacity);
  /** @brief Strict padded decode. Invalid data/capacity leaves output and
   * written unchanged. NULL source/output are allowed at length zero. Exact
   * in-place decoding is supported.
   */
  int mulib_base64_decode (const char *data, size_t n, unsigned char *out,
                           size_t capacity, size_t *written);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_BASE64_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-base64.h`](../include/mulib-base64.h) |
| Implementation switch | `MULIB_IMPL_BASE64` |
| Repository-local includes | None |
| Complete example | [`examples/modules/base64.c`](../examples/modules/base64.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/base64.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
