# MuLIB timer

[Manuals](README.md) · [Library index](index.md)

Portable elapsed-time measurements using C11 timespec_get. Uses a wall clock rather than a monotonic clock because ISO C11 does not define a monotonic portable source; elapsed values can be affected by clock adjustments.

## Contract and behavior

Start records `timespec_get(..., TIME_UTC)` and returns 1 on success
or 0 on failure. Call elapsed only after successful initialization. It returns
elapsed seconds as a double, or NaN for NULL state or clock-read failure.
This is a wall-clock timer, **not monotonic**: clock corrections can make elapsed
time negative or discontinuous. It owns no resources and requires no destructor;
it is unsuitable as a monotonic deadline source without an external policy.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_TIMER
#include "mulib-timer.h"
int main(void) {
    mulib_timer_t t;
    assert(mulib_timer_start(&t));
    assert(isfinite(mulib_timer_elapsed(&t)));
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-timer.h`](../include/mulib-timer.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-timer.h
 * @brief Portable elapsed-time measurements using C11 timespec_get.
 * @defgroup mulib_timer MuLIB timer
 * @{
 * @details Uses a wall clock rather than a monotonic clock because ISO C11
 * does not define a monotonic portable source; elapsed values can be affected
 * by clock adjustments.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_TIMER before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_TIMER_H
#define MULIB_TIMER_H
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

  /** @brief Portable elapsed-time measurements using C11 timespec_get. */
  typedef struct mulib_timer
  {
    struct timespec start;
  } mulib_timer_t;
  /** @brief mulib timer start; see manual/mulib-timer.md for contracts and
   * examples. */
  int mulib_timer_start (mulib_timer_t *t);
  /** @brief mulib timer elapsed; see manual/mulib-timer.md for contracts and
   * examples. */
  double mulib_timer_elapsed (const mulib_timer_t *t);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_TIMER_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-timer.h`](../include/mulib-timer.h) |
| Implementation switch | `MULIB_IMPL_TIMER` |
| Repository-local includes | None |
| Complete example | [`examples/modules/timer.c`](../examples/modules/timer.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/timer.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
