# MuLIB importgraph

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/importgraph.c)

Zero-initialize and set `nodes`. `add(dependency,consumer)` inserts an edge so the
dependency sorts before its consumer; duplicate insertion is idempotent. Both
indices must be below node count. Self edges are accepted but form cycles.

`order` emits every node, including isolated ones, choosing the smallest available
zero-indegree node at each step. Capacity must cover all nodes. Cycles, invalid
edges, insufficient capacity, and allocation failures return 0 with output
unchanged. Empty graphs succeed with NULL output. `free` resets the graph.
Ordering takes O(V²+VE) time and O(V) scratch; storage O(E).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-importgraph.h`](../include/mulib-importgraph.h) |
| Implementation switch | `MULIB_IMPL_IMPORTGRAPH` |
| Repository-local includes | None |
| Complete example | [`examples/modules/importgraph.c`](../examples/modules/importgraph.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/importgraph.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
