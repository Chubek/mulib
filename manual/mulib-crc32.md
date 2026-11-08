# MuLIB crc32

[Manuals](README.md) · [Library index](index.md)

IEEE CRC-32/ISO-HDLC checksum with incremental updates. The API returns an
integer value; choose serialization byte order separately when writing a format.

## Contract and behavior

The reflected IEEE CRC-32 polynomial is `0xedb88320`. Start streaming
with seed **0**; pass each finalized result to `update()` for the next chunk.
The convenience function uses that seed, and empty input produces zero.
Results are checksum values, not statuses. NULL with nonzero length returns
the seed unchanged, so validate your input separately. The API allocates and
retains nothing. CRC-32 detects corruption; it is not a cryptographic MAC.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_CRC32
#include "mulib-crc32.h"
int main(void) {
    assert(mulib_crc32("123456789", 9) == 0xcbf43926u);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-crc32.h`](../include/mulib-crc32.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-crc32.h
 * @brief IEEE CRC-32 checksum with incremental updates.
 * @defgroup mulib_crc32 MuLIB crc32
 * @{
 * @details Use seed zero for a fresh CRC. Passing a completed CRC as seed
 * continues the same reflected CRC-32/ISO-HDLC checksum. Results are integers;
 * serialization byte order is chosen by the caller's format.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_CRC32 before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_CRC32_H
#define MULIB_CRC32_H
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

  /** @brief mulib crc32 update; see manual/mulib-crc32.md for contracts and
   * examples. */
  uint32_t mulib_crc32_update (uint32_t seed, const void *data, size_t n);
  /** @brief mulib crc32; see manual/mulib-crc32.md for contracts and examples.
   */
  uint32_t mulib_crc32 (const void *data, size_t n);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_CRC32_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-crc32.h`](../include/mulib-crc32.h) |
| Implementation switch | `MULIB_IMPL_CRC32` |
| Repository-local includes | None |
| Complete example | [`examples/modules/crc32.c`](../examples/modules/crc32.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/crc32.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
