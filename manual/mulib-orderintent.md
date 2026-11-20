# OrderIntent — Offline order preflight

Checks the proposed side, valid time, quantity, price grid and exact
maximum notional. Success means the input meets this arithmetic policy
only. It does not authorize a trade, reserve funds, load secrets, or send
an order. Venue rules and actual approvals belong in a separate layer.
See [FinPRED verification and limits](finpred.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-orderintent.h`](../include/mulib-orderintent.h) |
| Implementation switch | `MULIB_IMPL_ORDERINTENT` |
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
