# price-monitor

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Thresholded price change and availability event detection.

## Contract and supported profile

Comparison validates both observations as the same offer. A stock-bit change
takes precedence over price changes. Otherwise absolute minor-unit delta divided
by max(1,abs(old minor)) is compared inclusively to the finite nonnegative threshold.
Unchanged prices produce NO_CHANGE even at zero threshold. No timestamp ordering
is imposed; the caller identifies old/new observations. The event output is
unchanged on validation failure.

## Integration and ownership

Include `include/mulib-price-monitor.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_MONEY`, `MULIB_IMPL_PRICE_MONITOR`, `MULIB_IMPL_PRICE_OBSERVATION`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/price-monitor.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_pricing.c`](../tests/test_web_pricing.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-price-monitor.h`](../include/mulib-price-monitor.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-price-monitor.h
 * @brief Thresholded price change and availability event detection.
 * C11 single-header module. Define MULIB_IMPL_PRICE_MONITOR in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_PRICE_MONITOR_H
#define MULIB_PRICE_MONITOR_H
#include "mulib-price-observation.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum mulib_price_monitor_event
  {
    MULIB_PRICE_MONITOR_NO_CHANGE = 0,
    MULIB_PRICE_MONITOR_UP = 1,
    MULIB_PRICE_MONITOR_DOWN = 2,
    MULIB_PRICE_MONITOR_STOCK = 3
  } mulib_price_monitor_event_t;
  /** @brief See manual/mulib-price-monitor.md for this operation's contract.
   */
  int mulib_price_monitor_compare (const mulib_price_observation_t *old_item,
                                   const mulib_price_observation_t *new_item,
                                   double min_relative_change,
                                   mulib_price_monitor_event_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_MONITOR_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-price-monitor.h`](../include/mulib-price-monitor.h) |
| Implementation switch | `MULIB_IMPL_PRICE_MONITOR` |
| Repository-local includes | [mulib-price-observation](mulib-price-observation.md) |
| Complete example | [`examples/modules/price-monitor.c`](../examples/modules/price-monitor.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/price-monitor.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
