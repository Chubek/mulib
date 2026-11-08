# MuLIB varint

[Manuals](README.md) · [Library index](index.md)

Unsigned LEB128 integer encoding and bounds-checked decoding. Rejects overflow, truncation, and unterminated input. Non-minimal encodings are accepted and decode to the same unsigned integer.

## Contract and behavior

Encode writes unsigned base-128 little-endian groups, with a high-bit
continuation flag. Supply ten bytes; it returns 1..10 bytes, or zero for NULL
output. Zero encodes as one zero byte. Decode returns 1 on the first complete
value and optionally reports consumed bytes; trailing bytes remain unconsumed.
It returns 0 on truncation, values beyond 64 bits, or NULL required pointers,
preserving outputs. Nonminimal encodings are accepted. There is no signed
ZigZag conversion, allocation, or terminator.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_VARINT
#include "mulib-varint.h"
int main(void) {
    unsigned char b[10];
    size_t n = mulib_varint_encode(300, b);
    uint64_t value = 0;
    assert(mulib_varint_decode(b, n, &value, NULL) && value == 300);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-varint.h`](../include/mulib-varint.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-varint.h
 * @brief Unsigned LEB128 integer encoding and bounds-checked decoding.
 * @defgroup mulib_varint MuLIB varint
 * @{
 * @details Rejects overflow, truncation, and unterminated input. Non-minimal
 * encodings are accepted and decode to the same unsigned integer.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_VARINT before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_VARINT_H
#define MULIB_VARINT_H
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

  /** @brief mulib varint encode; see manual/mulib-varint.md for contracts and
   * examples. */
  size_t mulib_varint_encode (uint64_t value, unsigned char out[10]);
  /** @brief mulib varint decode; see manual/mulib-varint.md for contracts and
   * examples. */
  int mulib_varint_decode (const unsigned char *data, size_t length,
                           uint64_t *out, size_t *consumed);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_VARINT_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-varint.h`](../include/mulib-varint.h) |
| Implementation switch | `MULIB_IMPL_VARINT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/varint.c`](../examples/modules/varint.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/varint.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
