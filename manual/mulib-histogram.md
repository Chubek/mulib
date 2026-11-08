# MuLIB histogram

[Manuals](README.md) · [Library index](index.md)

Histogram accumulation over a fixed numeric range. Values outside [low, high] are rejected, and the high endpoint belongs to the final bin. Widths are uniform in floating-point units.

## Contract and behavior

Initialize with a positive bin count and finite bounds `low < high`.
Bins cover equal-width intervals; the upper bound is included in the final bin.
`add()` rejects nonfinite/out-of-range samples and refuses to overflow a bin
counter. Init/add return 1 on success and 0 on failure. Counters are owned
`size_t` values, not probabilities; normalization is caller-owned. `free()`
releases bins and zeros state. Free before reinitializing a live histogram.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_HISTOGRAM
#include "mulib-histogram.h"
int main(void) {
    mulib_histogram_t h;
    assert(mulib_histogram_init(&h, 5, 0, 5));
    assert(mulib_histogram_add(&h, 2.0));
    mulib_histogram_free(&h);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-histogram.h`](../include/mulib-histogram.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-histogram.h
 * @brief Histogram accumulation over a fixed numeric range.
 * @defgroup mulib_histogram MuLIB histogram
 * @{
 * @details Values outside [low, high] are rejected, and the high endpoint
 * belongs to the final bin. Widths are uniform in floating-point units.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_HISTOGRAM before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_HISTOGRAM_H
#define MULIB_HISTOGRAM_H
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

  /** @brief Histogram accumulation over a fixed numeric range. */
  typedef struct mulib_histogram
  {
    size_t *bins;
    size_t count;
    double low, high;
  } mulib_histogram_t;
  /** @brief Allocate bins over finite low < high; failure leaves the
   * destination unchanged. */
  int mulib_histogram_init (mulib_histogram_t *h, size_t bins, double low,
                            double high);
  /** @brief mulib histogram add; see manual/mulib-histogram.md for contracts
   * and examples. */
  int mulib_histogram_add (mulib_histogram_t *h, double value);
  /** @brief mulib histogram free; see manual/mulib-histogram.md for contracts
   * and examples. */
  void mulib_histogram_free (mulib_histogram_t *h);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_HISTOGRAM_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-histogram.h`](../include/mulib-histogram.h) |
| Implementation switch | `MULIB_IMPL_HISTOGRAM` |
| Repository-local includes | None |
| Complete example | [`examples/modules/histogram.c`](../examples/modules/histogram.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/histogram.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
