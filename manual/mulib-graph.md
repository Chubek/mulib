# MuLIB graph

[Manuals](README.md) · [Library index](index.md)

Directed graph with dense adjacency matrix and breadth-first search. Dense representation uses O(V²) space; BFS discovers each reachable vertex once, visiting adjacency rows in ascending vertex order.

## Contract and behavior

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
/**
 * @file mulib-graph.h
 * @brief Directed graph with dense adjacency matrix and breadth-first search.
 * @defgroup mulib_graph MuLIB graph
 * @{
 * @details Dense representation uses O(V²) space; BFS discovers each reachable
 * vertex once, visiting adjacency rows in ascending vertex order.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_GRAPH before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_GRAPH_H
#define MULIB_GRAPH_H
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Directed graph with dense adjacency matrix and breadth-first
   * search. */
  typedef struct mulib_graph
  {
    unsigned char *edges;
    size_t n;
  } mulib_graph_t;
  /** @brief mulib graph init; see manual/mulib-graph.md for contracts and
   * examples. */
  int mulib_graph_init (mulib_graph_t *g, size_t vertices);
  /** @brief mulib graph edge; see manual/mulib-graph.md for contracts and
   * examples. */
  int mulib_graph_edge (mulib_graph_t *g, size_t src, size_t dst, int enabled);
  /** @brief mulib graph bfs; see manual/mulib-graph.md for contracts and
   * examples. */
  int mulib_graph_bfs (const mulib_graph_t *g, size_t start, size_t *order,
                       size_t capacity);
  /** @brief mulib graph free; see manual/mulib-graph.md for contracts and
   * examples. */
  void mulib_graph_free (mulib_graph_t *g);

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
