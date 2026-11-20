# RateQuote — Exact full-stay charges

Use canonical Money with matching currency and decimals for all charges,
including zero values. Normalize a full-stay room price, required fees and
taxes; mark fees or taxes already included so they are not added again.
The payable amount is exact. The nightly average is rounded and must not
be multiplied back to recreate the bill.

Use `mulib_ratequote_normalize_dated` to check exact civil dates, booking
and observation times, typed cancellation and meal terms, ages, and an
optional nightly room-charge breakdown. It does not apply cancellation
fees or infer service charges. See [FinPRED verification and limits](finpred.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ratequote.h`](../include/mulib-ratequote.h) |
| Implementation switch | `MULIB_IMPL_RATEQUOTE` |
| Repository-local includes | [mulib-money](mulib-money.md) |
| Complete example | [`examples/hospitality_demo.c`](../examples/hospitality_demo.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/hospitality_demo.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
