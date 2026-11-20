# Backtest — Point-forecast holdout metrics

The replay scoring API checks that prediction inputs and the baseline were
known by the decision, and that each outcome was available by the label
cutoff. Invalid or unavailable rows fail rather than being omitted. MAE,
RMSE, signed bias, and WAPE are point-error metrics, not financial returns.
Training, fold construction, provenance, costs, and slippage are outside
the kernel. See [FinPRED verification and limits](finpred.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-backtest.h`](../include/mulib-backtest.h) |
| Implementation switch | `MULIB_IMPL_BACKTEST` |
| Repository-local includes | None |
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
