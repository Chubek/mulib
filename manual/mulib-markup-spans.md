# `mulib-markup-spans`

Own binary text and stand-off annotations on half-open byte ranges. Ranges
may cross; empty ranges can mark positions. Attributed run partitioning
provides a separate, owned result containing each run's active annotation IDs.
`mulib_cdd_mkir_spans_runs_limited` rejects output beyond the supplied
run and membership limits without publishing a partial collection.

See [Markup IR](markup-ir.md) for offset semantics, limits, and conversions.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-markup-spans.h`](../kernels/mulib-markup-spans.h) |
| Implementation switch | `MULIB_IMPL_CDD_MARKUP_SPANS` |
| Repository-local includes | None |
| Complete example | [`examples/markup-ir.c`](../examples/markup-ir.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/markup-ir.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
