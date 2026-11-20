# CompSet — Comparable property samples

Supply offers already normalized for the same booking scenario. Map
listings from all platforms to one canonical ID per property. CompSet
returns one fresh eligible offer per ID and can explain individual
exclusions. It does not infer equivalent properties, guest groups, or terms.

When exporting minor units into StatsProc, explicitly select either
unweighted Hyndman–Fan type 7 interpolation or the discrete weighted CDF.
Even equal weights do not make these estimators interchangeable. Do not
round a percentile until applying the application's money policy.
See [FinPRED verification and limits](finpred.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-compset.h`](../include/mulib-compset.h) |
| Implementation switch | `MULIB_IMPL_COMPSET` |
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
