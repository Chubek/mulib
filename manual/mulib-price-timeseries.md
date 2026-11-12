# price-timeseries

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Time-bucketed price series with missing-value flags and gap handling.

## Contract and supported profile

Build produces at most one million buckets at start+i*step, with nonnegative
start, positive step and checked timestamps. Values are mean minor units, not
major currency units. All history records must describe the same validated
offer. Buckets are half-open intervals; empty buckets have count=observed=0.
Build stages output and leaves it unchanged on failure. Interpolation requires
strictly increasing timestamps and finite nonnegative observed anchors; it uses
timestamp-weighted linear interpolation and nearest-anchor edge filling without
changing count/observed flags. No observed anchor is an error.

## Integration and ownership

Include `include/mulib-price-timeseries.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_MONEY`, `MULIB_IMPL_PRICE_HISTORY`, `MULIB_IMPL_PRICE_OBSERVATION`, `MULIB_IMPL_PRICE_TIMESERIES`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/price-timeseries.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_pricing.c`](../tests/test_web_pricing.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-price-timeseries.h`](../include/mulib-price-timeseries.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-price-timeseries.h
 * @brief Time-bucketed price series with missing-value flags and gap handling.
 * C11 single-header module. Define MULIB_IMPL_PRICE_TIMESERIES in exactly one
 * C translation unit.
 *
 */
#ifndef MULIB_PRICE_TIMESERIES_H
#define MULIB_PRICE_TIMESERIES_H
#include "mulib-price-history.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_price_point
  {
    int64_t time;
    double value;
    uint32_t count;
    int observed;
  } mulib_price_point_t;
  /** @brief See manual/mulib-price-timeseries.md for this operation's
   * contract. */
  int mulib_price_timeseries_build (const mulib_price_history_t *h,
                                    int64_t start, int64_t step,
                                    size_t buckets, mulib_price_point_t *out);
  /** @brief See manual/mulib-price-timeseries.md for this operation's
   * contract. */
  int mulib_price_timeseries_interpolate (mulib_price_point_t *series,
                                          size_t n);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_TIMESERIES_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-price-timeseries.h`](../include/mulib-price-timeseries.h) |
| Implementation switch | `MULIB_IMPL_PRICE_TIMESERIES` |
| Repository-local includes | [mulib-price-history](mulib-price-history.md) |
| Complete example | [`examples/modules/price-timeseries.c`](../examples/modules/price-timeseries.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/price-timeseries.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
