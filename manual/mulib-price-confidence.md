# price-confidence

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Overflow-aware weighted confidence primitives and median/MAD deviation scores.

## Contract and supported profile

`combine` is a normalized weighted mean of finite scores in [0,1], with
nonnegative finite weights and positive total weight. Weight rescaling avoids
overflow for large weights. `deviation` computes absolute distance from the median
divided by (1.4826 * median absolute deviation + 1e-9), not a probability. It copies
and sorts its finite inputs, using O(n) storage and O(n log n) time. Empty inputs,
allocation failures and nonfinite results fail without changing output. These
primitives do not supply empirical calibration by themselves.

## Integration and ownership

Include `include/mulib-price-confidence.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_PRICE_CONFIDENCE`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/price-confidence.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_pricing.c`](../tests/test_web_pricing.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-price-confidence.h`](../include/mulib-price-confidence.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-price-confidence.h
 * @brief Weighted confidence primitives and median/MAD deviation scores.
 * C11 single-header module. Define MULIB_IMPL_PRICE_CONFIDENCE in exactly one
 * C translation unit.
 *
 */
#ifndef MULIB_PRICE_CONFIDENCE_H
#define MULIB_PRICE_CONFIDENCE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-price-confidence.md for this operation's
   * contract. */
  int mulib_price_confidence_combine (const double *scores,
                                      const double *weights, size_t n,
                                      double *out);
  /** @brief See manual/mulib-price-confidence.md for this operation's
   * contract. */
  int mulib_price_confidence_deviation (const double *prices, size_t n,
                                        double candidate, double *score);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_CONFIDENCE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-price-confidence.h`](../include/mulib-price-confidence.h) |
| Implementation switch | `MULIB_IMPL_PRICE_CONFIDENCE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/price-confidence.c`](../examples/modules/price-confidence.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/price-confidence.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
