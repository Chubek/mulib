# MuLIB endian

[Manuals](README.md) · [Library index](index.md)

Explicit little- and big-endian primitive load/store helpers. Safe on unaligned byte buffers; unlike pointer casts these functions avoid alignment and strict-aliasing violations.

## Contract and behavior

Loads and stores operate on exactly four bytes for 32-bit values or
eight bytes for 64-bit values. Bytewise access supports unaligned addresses and
is independent of host byte order. A non-NULL pointer must provide the full
accessible range; there is no length parameter. NULL loads return zero and NULL
stores do nothing. No allocation occurs. These helpers serialize integers,
not C structure layouts.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_ENDIAN
#include "mulib-endian.h"
int main(void) {
    unsigned char b[4];
    mulib_endian_store_le32(b, 0x12345678u);
    assert(mulib_endian_load_le32(b) == 0x12345678u);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-endian.h`](../include/mulib-endian.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-endian.h
 * @brief Explicit little- and big-endian primitive load/store helpers.
 * @defgroup mulib_endian MuLIB endian
 * @{
 * @details Safe on unaligned byte buffers; unlike pointer casts these
 * functions avoid alignment and strict-aliasing violations.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_ENDIAN before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_ENDIAN_H
#define MULIB_ENDIAN_H
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

  /** @brief mulib endian load le32; see manual/mulib-endian.md for contracts
   * and examples. */
  uint32_t mulib_endian_load_le32 (const unsigned char *bytes);
  /** @brief mulib endian load be32; see manual/mulib-endian.md for contracts
   * and examples. */
  uint32_t mulib_endian_load_be32 (const unsigned char *bytes);
  /** @brief mulib endian store le32; see manual/mulib-endian.md for contracts
   * and examples. */
  void mulib_endian_store_le32 (unsigned char *bytes, uint32_t value);
  /** @brief mulib endian store be32; see manual/mulib-endian.md for contracts
   * and examples. */
  void mulib_endian_store_be32 (unsigned char *bytes, uint32_t value);
  /** Unaligned 64-bit counterparts; NULL loads zero and stores do nothing. */
  uint64_t mulib_endian_load_le64 (const unsigned char *bytes);
  uint64_t mulib_endian_load_be64 (const unsigned char *bytes);
  void mulib_endian_store_le64 (unsigned char *bytes, uint64_t value);
  void mulib_endian_store_be64 (unsigned char *bytes, uint64_t value);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ENDIAN_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-endian.h`](../include/mulib-endian.h) |
| Implementation switch | `MULIB_IMPL_ENDIAN` |
| Repository-local includes | None |
| Complete example | [`examples/modules/endian.c`](../examples/modules/endian.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/endian.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
