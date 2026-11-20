# FXRate — Rational conversion

Rates are target minor units per source minor unit, not major-unit
market rates. Match the source currency and precision exactly.
`mulib_fxrate_convert_asof` checks observation, availability, and age
before rounding with canonical Money. The caller chooses the rate, side,
and source; there is no triangulation. See [FinPRED verification and limits](finpred.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-fxrate.h`](../include/mulib-fxrate.h) |
| Implementation switch | `MULIB_IMPL_FXRATE` |
| Repository-local includes | [mulib-money](mulib-money.md) |
| Complete example | [`examples/finpred_market.c`](../examples/finpred_market.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/finpred_market.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
