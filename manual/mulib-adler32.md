# MuLIB adler32

[Manuals](README.md) · [Library index](index.md)

Adler-32 checksum with streaming seed support. Adler-32 is a fast corruption check, not a cryptographic integrity code. Initialize incremental checksums with seed 1.

## Contract and behavior

`mulib_adler32` starts with seed **1**. For streaming input, pass the
previous result to `mulib_adler32_update`; chunking does not change the checksum.
The result is a checksum value, not a success code. No storage is allocated or
retained. A NULL data pointer with nonzero length returns the seed unchanged;
it does not report invalid input separately. Lengths are bytes. This is a
corruption checksum, not authentication.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_ADLER32
#include "mulib-adler32.h"
int main(void) {
    assert(mulib_adler32("Wikipedia", 9) == 0x11e60398u);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-adler32.h`](../include/mulib-adler32.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-adler32.h
 * @brief Adler-32 checksum with streaming seed support.
 * @defgroup mulib_adler32 MuLIB adler32
 * @{
 * @details Adler-32 is a fast corruption check, not a cryptographic integrity
 * code. Initialize incremental checksums with seed 1.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_ADLER32 before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_ADLER32_H
#define MULIB_ADLER32_H
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

  /** @brief mulib adler32 update; see manual/mulib-adler32.md for contracts
   * and examples. */
  uint32_t mulib_adler32_update (uint32_t seed, const void *data,
                                 size_t length);
  /** @brief mulib adler32; see manual/mulib-adler32.md for contracts and
   * examples. */
  uint32_t mulib_adler32 (const void *data, size_t length);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ADLER32_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-adler32.h`](../include/mulib-adler32.h) |
| Implementation switch | `MULIB_IMPL_ADLER32` |
| Repository-local includes | None |
| Complete example | [`examples/modules/adler32.c`](../examples/modules/adler32.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/adler32.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
