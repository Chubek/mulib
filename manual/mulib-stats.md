# MuLIB stats

[Manuals](README.md) · [Library index](index.md)

Streaming sample statistics using Welford numerical updates. Variance is the sample variance (denominator n-1); fewer than two samples returns NAN.

## Contract and behavior

Call `init()` before adding samples: it sets count/mean/M2 to zero,
minimum to +infinity, and maximum to -infinity. Add uses Welford's online update
in constant time and refuses to increment beyond `SIZE_MAX` samples. Nonfinite
samples are not rejected. Variance is **sample variance**, dividing by n−1;
variance and standard deviation return NaN for fewer than two samples or NULL
state. No sample array is retained and no allocation occurs.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_STATS
#include "mulib-stats.h"
int main(void) {
    mulib_stats_t s;
    mulib_stats_init(&s);
    mulib_stats_add(&s, 2);
    mulib_stats_add(&s, 4);
    assert(s.mean == 3);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-stats.h`](../include/mulib-stats.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-stats.h
 * @brief Streaming sample statistics using Welford numerical updates.
 * @defgroup mulib_stats MuLIB stats
 * @{
 * @details Variance is the sample variance (denominator n-1); fewer than two
 * samples returns NAN.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_STATS before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_STATS_H
#define MULIB_STATS_H
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

  /** @brief Streaming sample statistics using Welford numerical updates. */
  typedef struct mulib_stats
  {
    size_t count;
    double mean, m2, min, max;
  } mulib_stats_t;
  /** @brief mulib stats init; see manual/mulib-stats.md for contracts and
   * examples. */
  void mulib_stats_init (mulib_stats_t *s);
  /** @brief mulib stats add; see manual/mulib-stats.md for contracts and
   * examples. */
  void mulib_stats_add (mulib_stats_t *s, double value);
  /** @brief mulib stats variance; see manual/mulib-stats.md for contracts and
   * examples. */
  double mulib_stats_variance (const mulib_stats_t *s);
  /** @brief mulib stats stddev; see manual/mulib-stats.md for contracts and
   * examples. */
  double mulib_stats_stddev (const mulib_stats_t *s);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_STATS_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-stats.h`](../include/mulib-stats.h) |
| Implementation switch | `MULIB_IMPL_STATS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/stats.c`](../examples/modules/stats.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/stats.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
