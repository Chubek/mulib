# MuLIB graph

[Manuals](README.md) · [Library index](index.md)

Directed graph with dense adjacency matrix and breadth-first search. Dense representation uses O(V²) space; BFS discovers each reachable vertex once, visiting adjacency rows in ascending vertex order.

## Contract and behavior

`toposort()` emits all vertices in dependency order, choosing the smallest
zero-indegree vertex at each step. It returns 1 on success, 0 for cycles,
insufficient capacity, invalid arguments, or allocation failure. Output is
unchanged on failure. An initialized zero-vertex graph accepts NULL output.
Time is O(V²), temporary storage O(V).

`reverse()` creates an independent transposed adjacency matrix. Its output must
be zero-initialized and distinct from the source. Failure leaves output unchanged;
free both graphs independently. Time and storage are O(V²).

`init()` allocates a zeroed directed adjacency matrix with one byte
per vertex pair, requiring O(n²) storage. `edge(a,b,on)` changes only a→b;
set both directions for an undirected graph. Init/edge return 1 on success,
0 on failure. `free()` releases the matrix and zeros the graph.

`bfs()` requires output capacity for all n vertices, even if fewer are reachable.
It returns the reachable count or -1 on invalid input, allocation failure, or
a count beyond `INT_MAX`. It visits neighbors in increasing index order and
uses O(n) temporary storage; matrix scanning costs O(n²) in the worst case.
The count-overflow case can occur after output has been written.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_GRAPH
#include "mulib-graph.h"
int main(void) {
    mulib_graph_t g;
    assert(mulib_graph_init(&g, 2));
    assert(mulib_graph_edge(&g, 0, 1, 1));
    size_t visited[2];
    assert(mulib_graph_bfs(&g, 0, visited, 2) == 2);
    mulib_graph_free(&g);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-graph.h`](../include/mulib-graph.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB compiler tools: Patched dense directed graph with BFS, deterministic topological ordering and reversed edges.
 * ISO C11 single-header module. Define MULIB_IMPL_GRAPH in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_GRAPH_H
#define MULIB_GRAPH_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_graph {unsigned char *edges;
size_t n;
} mulib_graph_t;
int mulib_graph_init(mulib_graph_t *g,size_t vertices);
int mulib_graph_edge(mulib_graph_t *g,size_t src,size_t dst,int enabled);
int mulib_graph_bfs(const mulib_graph_t *g,size_t start,size_t *order,size_t capacity);
void mulib_graph_free(mulib_graph_t *g);
/* Returns 1 for DAG; 0 for cycles/invalid/OOM; output unchanged on failure. */
int mulib_graph_toposort(const mulib_graph_t *g,size_t *order,size_t capacity);
/* Creates deep-copy reversal; out must be zero initialized. */
int mulib_graph_reverse(const mulib_graph_t *g,mulib_graph_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_GRAPH_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-graph.h`](../include/mulib-graph.h) |
| Implementation switch | `MULIB_IMPL_GRAPH` |
| Repository-local includes | None |
| Complete example | [`examples/modules/graph.c`](../examples/modules/graph.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/graph.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
