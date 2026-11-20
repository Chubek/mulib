# MarketQuote — Level-one quote checks

Validate a snapshot with matched Money units, noncrossed prices and
nonnegative microsecond timestamps. `mulib_marketquote_usable_at` also
checks receipt and age at a cutoff and a caller-supplied integer tick.
Spread is exact; midpoint rounds down in minor units. The host owns venue
conventions and feed authorization. See [FinPRED verification and limits](finpred.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-marketquote.h`](../include/mulib-marketquote.h) |
| Implementation switch | `MULIB_IMPL_MARKETQUOTE` |
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
