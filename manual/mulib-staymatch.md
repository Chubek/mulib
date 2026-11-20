# StayMatch — Exact bounded room selection

Each call covers one property. Use `mulib_staymatch_solve_dated` for dated
offers with explicit booking windows, child ages, adult minima, and shared
inventory groups. Offers for other stay dates are excluded, not inferred.
Prices are per room for the exact stay, in matching Money units.

Results rank by total price, room count, then counts in ascending room-type
ID order for the dated API. The legacy API uses input order for ties.
In both, `selected[]` indexes the caller's array. Budget exhaustion is a
failure that preserves results and count; successful completion can find
zero feasible bundles. See [FinPRED verification and limits](finpred.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-staymatch.h`](../include/mulib-staymatch.h) |
| Implementation switch | `MULIB_IMPL_STAYMATCH` |
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
