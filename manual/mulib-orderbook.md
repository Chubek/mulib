# OrderBook — Level-two snapshot and updates

Initialize caller-owned storage and optionally load a validated, ordered
snapshot. Incremental updates must advance the last sequence by exactly
one. Rejecting a cross, sequence gap, or missing delete leaves the book
unchanged. The host must serialize access and authorize sequence recovery;
there is no venue protocol or trade matching. See [FinPRED verification and limits](finpred.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-orderbook.h`](../include/mulib-orderbook.h) |
| Implementation switch | `MULIB_IMPL_ORDERBOOK` |
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
