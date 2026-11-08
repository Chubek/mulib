# MuLIB hash

[Manuals](README.md) · [Library index](index.md)

Fast non-cryptographic FNV-1a hashing for raw buffers and text. FNV-1a is useful for hash tables and checksums where adversaries do not control inputs. It provides no collision resistance.

## Contract and behavior

`fnv1a()` computes 64-bit FNV-1a over explicit bytes; `string()` hashes
through the first NUL. Empty input returns the FNV offset basis
`14695981039346656037`. NULL with nonzero byte length, or a NULL string, returns
zero without a separate error status. Hash collisions are possible, and this
is not a keyed or cryptographic hash. No storage is allocated or retained;
results do not depend on host endianness.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_HASH
#include "mulib-hash.h"
int main(void) {
    assert(mulib_hash_string("hello") == mulib_hash_fnv1a("hello", 5));
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-hash.h`](../include/mulib-hash.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-hash.h
 * @brief Fast non-cryptographic FNV-1a hashing for raw buffers and text.
 * @defgroup mulib_hash MuLIB hash
 * @{
 * @details FNV-1a is useful for hash tables and checksums where adversaries do
 * not control inputs. It provides no collision resistance.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_HASH before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_HASH_H
#define MULIB_HASH_H
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

  /** @brief mulib hash fnv1a; see manual/mulib-hash.md for contracts and
   * examples. */
  uint64_t mulib_hash_fnv1a (const void *data, size_t length);
  /** @brief mulib hash string; see manual/mulib-hash.md for contracts and
   * examples. */
  uint64_t mulib_hash_string (const char *text);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_HASH_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-hash.h`](../include/mulib-hash.h) |
| Implementation switch | `MULIB_IMPL_HASH` |
| Repository-local includes | None |
| Complete example | [`examples/modules/hash.c`](../examples/modules/hash.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/hash.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
