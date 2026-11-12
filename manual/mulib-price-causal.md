# price-causal

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Difference-in-differences average treatment effect diagnostic.

## Contract and supported profile

Difference-in-differences takes matched before/after samples within each
group, requiring at least two finite pairs per group. Effect is mean treated
change minus mean control change. Standard error is the square root of the sum
of each group's sample change variance divided by group count. Output includes
sample sizes and remains unchanged on failure. O(nt+nc), no allocation. Parallel
trends, independence and identification must be assessed externally; this helper
does not establish causation or supply hypothesis tests.

## Integration and ownership

Include `include/mulib-price-causal.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_PRICE_CAUSAL`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/price-causal.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_pricing.c`](../tests/test_web_pricing.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-price-causal.h`](../include/mulib-price-causal.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-price-causal.h
 * @brief Difference-in-differences average treatment effect diagnostic.
 * C11 single-header module. Define MULIB_IMPL_PRICE_CAUSAL in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_PRICE_CAUSAL_H
#define MULIB_PRICE_CAUSAL_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_price_causal_result
  {
    double effect, standard_error;
    size_t treated_n, control_n;
  } mulib_price_causal_result_t;
  /* Each array contains matched before/after observations for its group.
   * Requires >=2 per group. */
  int mulib_price_causal_did (const double *treated_before,
                              const double *treated_after, size_t nt,
                              const double *control_before,
                              const double *control_after, size_t nc,
                              mulib_price_causal_result_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_CAUSAL_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-price-causal.h`](../include/mulib-price-causal.h) |
| Implementation switch | `MULIB_IMPL_PRICE_CAUSAL` |
| Repository-local includes | None |
| Complete example | [`examples/modules/price-causal.c`](../examples/modules/price-causal.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/price-causal.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
