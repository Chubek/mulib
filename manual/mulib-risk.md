# Risk — Bounded snapshot guardrails

Checks gross market value, position count and a maximum single-name share
of gross. A successful evaluation can deny the snapshot; inspect
`allowed` separately from the function return. Concentration compares
integer minor units against an exact floor. This is not a margin model,
pre-trade simulation, or trading authority. See [FinPRED verification and limits](finpred.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-risk.h`](../kernels/mulib-risk.h) |
| Implementation switch | `MULIB_IMPL_RISK` |
| Repository-local includes | [mulib-portfolio](mulib-portfolio.md) |
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
