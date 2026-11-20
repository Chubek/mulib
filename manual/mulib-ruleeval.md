# RuleEval — Minor-unit policy rules

Obtain labeled quantiles from the same market snapshot and estimator.
For a P50-to-P40 shortcut use 5000 target basis points and a 1000-point
offset. To trigger strictly below €5 for EUR at scale 2, choose the strict
comparison and threshold 500. Pass an independently set hard floor and pick
clamp or skip-below-floor precedence. Infeasible movement and tick
constraints fail without a recommendation. This API is not a strategy DSL
or permission to publish a price. See [FinPRED verification and limits](finpred.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ruleeval.h`](../include/mulib-ruleeval.h) |
| Implementation switch | `MULIB_IMPL_RULEEVAL` |
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
