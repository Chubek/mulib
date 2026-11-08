# MuLIB hex

[Manuals](README.md) · [Library index](index.md)

Lowercase hexadecimal encoding and case-insensitive decoding. Output buffers must have capacity for every encoded or decoded byte. Decoder does not accept prefixes such as 0x or whitespace.

## Contract and behavior

Encoding emits lowercase hex and a NUL terminator. Provide `2*n + 1`
bytes using overflow-checked sizing. Return value is the number of hex characters
excluding NUL; zero can mean empty success or failure. Capacity is checked
before writing.

Decoding accepts either hex case and requires an even character count, with no
whitespace or separators. It writes exactly `n/2` bytes without a terminator.
Return is 1 on success, 0 on malformed input/capacity failure; output is unchanged
on failure. No allocations occur; use separate source and destination storage.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_HEX
#include "mulib-hex.h"
int main(void) {
    char s[16];
    assert(mulib_hex_encode("Hi", 2, s, sizeof s) == 4);
    assert(!strcmp(s, "4869"));
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-hex.h`](../include/mulib-hex.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-hex.h
 * @brief Lowercase hexadecimal encoding and case-insensitive decoding.
 * @defgroup mulib_hex MuLIB hex
 * @{
 * @details Output buffers must have capacity for every encoded or decoded
 * byte. Decoder does not accept prefixes such as 0x or whitespace.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_HEX before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_HEX_H
#define MULIB_HEX_H
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

  /** @brief mulib hex encode; see manual/mulib-hex.md for contracts and
   * examples. */
  size_t mulib_hex_encode (const void *data, size_t n, char *out, size_t cap);
  /** @brief Decode even-length hex, leaving output unchanged on error; exact
   * in-place is safe. */
  int mulib_hex_decode (const char *source, size_t n, unsigned char *out,
                        size_t cap);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_HEX_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-hex.h`](../include/mulib-hex.h) |
| Implementation switch | `MULIB_IMPL_HEX` |
| Repository-local includes | None |
| Complete example | [`examples/modules/hex.c`](../examples/modules/hex.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/hex.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
