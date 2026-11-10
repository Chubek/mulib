# MuLIB regalloc-graph

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/regalloc-graph.c)

`mulib_regalloc_graph` colors up to 64 nodes represented by adjacency bitsets.
Edges must be symmetric, contain no self bits, and refer only to nodes in range.
At each step the highest-degree uncolored node is selected (lowest index on ties)
and receives the lowest available register not used by an already colored neighbor.
Uncolorable nodes get -1 and increment spills. Zero registers spills every node.

This deterministic greedy algorithm is not an optimal graph-coloring solver.
Output colors/spill count are unchanged on invalid input. Empty NULL arrays are
accepted. There is no heap allocation; time O(V³) with V <= 64, fixed O(V) scratch.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-regalloc-graph.h`](../include/mulib-regalloc-graph.h) |
| Implementation switch | `MULIB_IMPL_REGALLOC_GRAPH` |
| Repository-local includes | None |
| Complete example | [`examples/modules/regalloc-graph.c`](../examples/modules/regalloc-graph.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/regalloc-graph.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
