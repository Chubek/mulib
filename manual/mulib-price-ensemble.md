# price-ensemble

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Error-weighted forecast blending with intermodel dispersion.

## Contract and supported profile

Blend weights finite nonnegative forecasts by 1/(MAE+1e-9), where each MAE
is finite and nonnegative. It returns the weighted mean and population standard
deviation across models, not a calibrated predictive interval. At least one
model and distinct output pointers are required. Nonfinite aggregate results
fail with both outputs unchanged. Computation is O(n) without allocations.

## Integration and ownership

Include `include/mulib-price-ensemble.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_PRICE_ENSEMBLE`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/price-ensemble.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_pricing.c`](../tests/test_web_pricing.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-price-ensemble.h`](../include/mulib-price-ensemble.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-price-ensemble.h
 * @brief Error-weighted forecast blending with intermodel dispersion.
 * C11 single-header module. Define MULIB_IMPL_PRICE_ENSEMBLE in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_PRICE_ENSEMBLE_H
#define MULIB_PRICE_ENSEMBLE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-price-ensemble.md for this operation's contract.
   */
  int mulib_price_ensemble_blend (const double *forecasts, const double *maes,
                                  size_t n, double *mean, double *stddev);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_ENSEMBLE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-price-ensemble.h`](../include/mulib-price-ensemble.h) |
| Implementation switch | `MULIB_IMPL_PRICE_ENSEMBLE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/price-ensemble.c`](../examples/modules/price-ensemble.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/price-ensemble.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
