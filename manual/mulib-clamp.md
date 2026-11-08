# MuLIB clamp

[Manuals](README.md) · [Library index](index.md)

Saturating arithmetic and interval clamping for signed integers. Addition/subtraction saturate at INT64_MIN or INT64_MAX on overflow. Clamp expects low<=high.

## Contract and behavior

`mulib_clamp_i64(x, lo, hi)` requires `lo <= hi`; it returns the nearest
endpoint when x is outside that interval. `add_i64` and `sub_i64` saturate at
`INT64_MIN` or `INT64_MAX` instead of overflowing signed arithmetic. Results are
numeric values, with no separate overflow indication. These functions allocate
nothing and retain no state.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_CLAMP
#include "mulib-clamp.h"
int main(void) {
    assert(mulib_clamp_i64(20, 0, 10) == 10);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-clamp.h`](../include/mulib-clamp.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-clamp.h
 * @brief Saturating arithmetic and interval clamping for signed integers.
 * @defgroup mulib_clamp MuLIB clamp
 * @{
 * @details Addition/subtraction saturate at INT64_MIN or INT64_MAX on
 * overflow. Clamp expects low<=high.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_CLAMP before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_CLAMP_H
#define MULIB_CLAMP_H
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

  /** @brief mulib clamp i64; see manual/mulib-clamp.md for contracts and
   * examples. */
  int64_t mulib_clamp_i64 (int64_t value, int64_t low, int64_t high);
  /** @brief mulib clamp add i64; see manual/mulib-clamp.md for contracts and
   * examples. */
  int64_t mulib_clamp_add_i64 (int64_t a, int64_t b);
  /** @brief mulib clamp sub i64; see manual/mulib-clamp.md for contracts and
   * examples. */
  int64_t mulib_clamp_sub_i64 (int64_t a, int64_t b);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_CLAMP_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-clamp.h`](../include/mulib-clamp.h) |
| Implementation switch | `MULIB_IMPL_CLAMP` |
| Repository-local includes | None |
| Complete example | [`examples/modules/clamp.c`](../examples/modules/clamp.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/clamp.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
