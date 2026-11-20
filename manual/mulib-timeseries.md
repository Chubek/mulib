# TimeSeries — Bounded point history

Supply immutable points ordered by strictly increasing event time.
Latest and SMA queries filter event and knowledge microsecond clocks
independently. The SMA is an average over the requested number of visible
points, not an elapsed-time weighted mean. There is no storage, resampling,
or corporate-action adjustment. See [FinPRED verification and limits](finpred.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-timeseries.h`](../include/mulib-timeseries.h) |
| Implementation switch | `MULIB_IMPL_TIMESERIES` |
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
