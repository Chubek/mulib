# Vantage-point metric trees

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_VANGAGE_POINT_TREES`; link `-lm`.
[Example](../examples/modules/vangage_point_trees.c). Filename spelling retained.

Zero-initialize or init uninitialized storage. Build copies fixed-stride points
and constructs balanced median-distance partitions, retaining original indices.
Stride must support the metric's alignment for every point (e.g. sizeof(type)).
Pointer members are shallow copies: pointed-to data remains caller-owned.
Metric/context are borrowed, deterministic and immutable for tree lifetime.
Require finite nonnegative symmetric distances obeying the triangle inequality;
these metric laws are not verified. Bad returned distances fail explicitly.

Build returns 0 on invalid/OOM/bad metric, preserving the old tree. Empty build
works with NULL points and replaces it with an empty tree. Search takes a finite
nonnegative radius; callback borrows points, must not mutate tree/context,
nonzero continues. Return counts include stop; SIZE_MAX invalid/bad metric,
possibly after earlier callbacks. Free resets ownership; no shallow tree copies.
Build uses O(n) auxiliary records and O(n log²n) sorting time; query uses O(log n)
recursive stack and can visit every point. No nearest-neighbor/k-NN API.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-vangage_point_trees.h`](../include/mulib-vangage_point_trees.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-vangage_point_trees.h
 * @brief Balanced metric vantage-point trees over copied fixed-stride points.
 * Define MULIB_IMPL_VANGAGE_POINT_TREES; link libm. Filename retained.
 */
#ifndef MULIB_VANGAGE_POINT_TREES_H
#define MULIB_VANGAGE_POINT_TREES_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef double (*mulib_vp_metric_fn)(const void *a, const void *b, void *context);
typedef int (*mulib_vp_visit_fn)(size_t original_index, const void *point, double distance, void *context);
typedef struct mulib_vp_node mulib_vp_node_t;
typedef struct mulib_vp_tree {
  unsigned char *points;
  mulib_vp_node_t *nodes;
  size_t size, stride;
  mulib_vp_metric_fn metric;
  void *context;
} mulib_vp_tree_t;
/** Initialize uninitialized storage; zero initialization also works. */
void mulib_vp_tree_init(mulib_vp_tree_t *tree);
/** Copy points and build a balanced tree. Metric/context are borrowed; metric
 * must be deterministic, finite, nonnegative and obey the triangle inequality.
 * Returns 0 on invalid/OOM/bad metric, preserving the old tree. */
int mulib_vp_tree_build(mulib_vp_tree_t *tree, const void *points, size_t n, size_t stride,
                        mulib_vp_metric_fn metric, void *context);
/** Radius query. Nonzero callback continues; no mutation from callbacks.
 * Return calls including stopping call, SIZE_MAX on invalid input/bad metric.
 * Bad metric may be reported after callbacks. Original indices survive building. */
size_t mulib_vp_tree_search(const mulib_vp_tree_t *tree, const void *query, double radius,
                           mulib_vp_visit_fn visit, void *context);
/** Release copied points and nodes, reset. */
void mulib_vp_tree_free(mulib_vp_tree_t *tree);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-vangage_point_trees.h`](../include/mulib-vangage_point_trees.h) |
| Implementation switch | `MULIB_IMPL_VANGAGE_POINT_TREES` |
| Repository-local includes | None |
| Complete example | [`examples/modules/vangage_point_trees.c`](../examples/modules/vangage_point_trees.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/vangage_point_trees.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
