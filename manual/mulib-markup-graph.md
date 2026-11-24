# `mulib-markup-graph`

Own a sparse directed graph of stable vertices, typed links, hyperedges and
properties. Optional DAG mode rejects a directed edge that would form a
cycle. Hyperedges describe membership, not traversal arcs. Reachability and
topological ordering operate on the directed links.

See [Markup IR](markup-ir.md) for graph ownership and conversion contracts.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-markup-graph.h`](../kernels/mulib-markup-graph.h) |
| Implementation switch | `MULIB_IMPL_CDD_MARKUP_GRAPH` |
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
