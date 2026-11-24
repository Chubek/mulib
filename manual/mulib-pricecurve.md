# PriceCurve — Time-to-event percentile

`mulib_pricecurve_evaluate` produces a percentile, not a price. Linear,
power and exponential modes interpolate over a positive horizon; values
beyond it use the far endpoint. Piecewise mode requires strictly
increasing times and nondecreasing percentiles, and clamps at the last knot.
Calendar conversion and hysteresis are host policies.
See [FinPRED verification and limits](finpred.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-pricecurve.h`](../kernels/mulib-pricecurve.h) |
| Implementation switch | `MULIB_IMPL_PRICECURVE` |
| Repository-local includes | None |
| Complete example | [`examples/hospitality_demo.c`](../examples/hospitality_demo.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/hospitality_demo.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
