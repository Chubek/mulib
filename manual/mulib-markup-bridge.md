# `mulib-markup-bridge`

Convert between markup trees, stand-off spans, and graphs. Tree conversion
from ranges requires nonempty, nested intervals; graph conversion preserves
crossing memberships. The hybrid form owns its tree, text/ranges and graph
independently.

Use `mulib_cdd_mkir_spans_to_graph_limited` to cap attributed-run expansion.
A failed `mulib_mkir_hybrid_index` rebuild leaves the old graph and index
offsets intact; a successful rebuild discards old custom links.

See [Markup IR](markup-ir.md) for conversion lossiness, ordering, ownership,
and implementation setup.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-markup-bridge.h`](../include/mulib-markup-bridge.h) |
| Implementation switch | `MULIB_IMPL_CDD_MARKUP_BRIDGE` |
| Repository-local includes | [mulib-markup-graph](mulib-markup-graph.md), [mulib-markup-spans](mulib-markup-spans.md), [mulib-markup-stream](mulib-markup-stream.md), [mulib-markup-tree](mulib-markup-tree.md) |
| Complete example | [`examples/markup-ir.c`](../examples/markup-ir.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/markup-ir.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
