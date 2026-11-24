# Portfolio — Snapshot valuation

Supply unique instruments with integer quantities and mark/cost values in
one currency and scale. Negative quantity denotes a short. The kernel
computes checked unrealized P&L and gross market value. Use the gross-only
operation when P&L is not required. There are no lots, realized gains,
fees or account balances. See [FinPRED verification and limits](finpred.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-portfolio.h`](../kernels/mulib-portfolio.h) |
| Implementation switch | `MULIB_IMPL_PORTFOLIO` |
| Repository-local includes | [mulib-money](mulib-money.md) |
| Complete example | [`examples/finpred_market.c`](../examples/finpred_market.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/finpred_market.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
