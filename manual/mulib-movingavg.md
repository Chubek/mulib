# MuLIB movingavg

[Manuals](README.md) · [Library index](index.md)

Constant-memory fixed-window moving average over doubles. Uses a circular window of exactly `capacity` recent values and updates its sum in constant time. Very long streams may accumulate floating-point summation drift.

## Contract and behavior

Initialize fresh storage with a positive window capacity; init returns
1 on success and 0 on size/allocation failure. Push retains the last capacity
samples, replacing the oldest when full, and returns their arithmetic mean.
During startup the divisor is the number received, not the full capacity.
An uninitialized/NULL state returns NaN. Nonfinite samples are not rejected and
can poison the running sum. `free()` releases samples and zeros state; free
before reinitializing. Each push takes constant time.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_MOVINGAVG
#include "mulib-movingavg.h"
int main(void) {
    mulib_movingavg_t m;
    assert(mulib_movingavg_init(&m, 3));
    assert(mulib_movingavg_push(&m, 4) == 4);
    mulib_movingavg_free(&m);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-movingavg.h`](../include/mulib-movingavg.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-movingavg.h
 * @brief Constant-memory fixed-window moving average over doubles.
 * @defgroup mulib_movingavg MuLIB movingavg
 * @{
 * @details Uses a circular window of exactly `capacity` recent values and
 * updates its sum in constant time. Very long streams may accumulate
 * floating-point summation drift.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_MOVINGAVG before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_MOVINGAVG_H
#define MULIB_MOVINGAVG_H
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

  /** @brief Constant-memory fixed-window moving average over doubles. */
  typedef struct mulib_movingavg
  {
    double *values, sum;
    size_t capacity, length, position;
  } mulib_movingavg_t;
  /** @brief mulib movingavg init; see manual/mulib-movingavg.md for contracts
   * and examples. */
  int mulib_movingavg_init (mulib_movingavg_t *m, size_t window);
  /** @brief mulib movingavg push; see manual/mulib-movingavg.md for contracts
   * and examples. */
  double mulib_movingavg_push (mulib_movingavg_t *m, double value);
  /** @brief mulib movingavg free; see manual/mulib-movingavg.md for contracts
   * and examples. */
  void mulib_movingavg_free (mulib_movingavg_t *m);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_MOVINGAVG_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-movingavg.h`](../include/mulib-movingavg.h) |
| Implementation switch | `MULIB_IMPL_MOVINGAVG` |
| Repository-local includes | None |
| Complete example | [`examples/modules/movingavg.c`](../examples/modules/movingavg.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/movingavg.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
