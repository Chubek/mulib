# CorporateActions — Split and dividend arithmetic

An exact split succeeds only if the share count remains integral and
representable. Price adjustment applies the inverse split with canonical
Money rounding; cash dividends multiply the per-share amount by an
integral share count. Fractional shares, cash-in-lieu, event dates and
multi-event policy are outside this API. See [FinPRED verification and limits](finpred.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-corporate-actions.h`](../include/mulib-corporate-actions.h) |
| Implementation switch | `MULIB_IMPL_CORPORATE_ACTIONS` |
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
