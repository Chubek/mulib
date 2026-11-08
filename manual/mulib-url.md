# MuLIB url

[Manuals](README.md) · [Library index](index.md)

URL component percent encoder and decoder. Implements percent encoding of individual URL components using the unreserved character set. It does not parse full URLs and does not translate `+` to space.

## Contract and behavior

Encode percent-escapes bytes except ASCII letters/digits and `-_.~`,
using uppercase hex. Capacity includes a trailing NUL; return length excludes
it. Zero can mean empty success or failure. Capacity failure may leave partial,
unterminated output; worst-case capacity is `3*n + 1` with checked arithmetic.

Decode accepts either hex case, copies `+` literally (not as space), and emits
binary bytes without a terminator. It returns 1 on success or 0 on malformed
escapes/capacity failure, preserving output and optional written count on
failure. No allocation or URL structure parsing occurs.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_URL
#include "mulib-url.h"
int main(void) {
    char out[32];
    assert(mulib_url_encode((const unsigned char *)"A B", 3, out, sizeof out) ==
           5);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-url.h`](../include/mulib-url.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-url.h
 * @brief URL component percent encoder and decoder.
 * @defgroup mulib_url MuLIB url
 * @{
 * @details Implements percent encoding of individual URL components using the
 * unreserved character set. It does not parse full URLs and does not translate
 * `+` to space.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_URL before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_URL_H
#define MULIB_URL_H
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

  /** @brief mulib url encode; see manual/mulib-url.md for contracts and
   * examples. */
  size_t mulib_url_encode (const unsigned char *s, size_t n, char *out,
                           size_t cap);
  /** @brief Decode percent escapes, leaving output/written unchanged on
   * failure. NULL source/output are allowed at length zero; exact in-place
   * decoding is safe.
   */
  int mulib_url_decode (const char *s, size_t n, unsigned char *out,
                        size_t cap, size_t *written);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_URL_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-url.h`](../include/mulib-url.h) |
| Implementation switch | `MULIB_IMPL_URL` |
| Repository-local includes | None |
| Complete example | [`examples/modules/url.c`](../examples/modules/url.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/url.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
