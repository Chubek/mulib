# BK spelling tree

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_BK_TREES` and `MULIB_IMPL_LEVENSHTEIN`.
[Example](../examples/modules/bk-trees.c).

Zero-initialize or init uninitialized storage. Insert copies NUL-terminated
byte strings and ignores duplicates. Invalid/OOM fails without mutation;
successful insert returns 1. Empty string is valid. Free iteratively resets
the tree. Do not shallow-copy live trees.

Search applies Levenshtein triangle-inequality pruning on child edge distances.
Callback borrows each word and distance, must not mutate tree; nonzero continues.
Return counts include stopping call; SIZE_MAX means invalid/OOM (possibly after
earlier callbacks if a distance workspace allocation fails). Order unspecified.
Query allocates O(tree size) pointers plus distance workspace. Tree is unbalanced
and may degrade to linear visits, each with quadratic edit-distance work.
No Unicode normalization or weighted/nonmetric distance substitution is allowed.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-bk-trees.h`](../include/mulib-bk-trees.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-bk-trees.h
 * @brief Owned BK tree for byte-string Levenshtein radius queries.
 * Define MULIB_IMPL_BK_TREES and MULIB_IMPL_LEVENSHTEIN.
 */
#ifndef MULIB_BK_TREES_H
#define MULIB_BK_TREES_H
#include "mulib-levenshtein.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_bk_node mulib_bk_node_t;
typedef struct mulib_bk_tree { mulib_bk_node_t *root; size_t size; } mulib_bk_tree_t;
typedef int (*mulib_bk_visit_fn)(const char *word, size_t distance, void *context);
/** Initialize uninitialized storage; zero initialization is also valid. */
void mulib_bk_tree_init(mulib_bk_tree_t *tree);
/** Copy word, ignoring duplicates; return 0 on invalid/OOM without mutation. */
int mulib_bk_tree_insert(mulib_bk_tree_t *tree, const char *word);
/** Metric-pruned radius query; callback borrows word and must not mutate tree.
 * Nonzero continues. Return calls including stop, SIZE_MAX invalid/OOM.
 * A distance-allocation failure may occur after earlier callbacks. Order unspecified. */
size_t mulib_bk_tree_search(const mulib_bk_tree_t *tree, const char *query, size_t radius,
                           mulib_bk_visit_fn visit, void *context);
/** Release iteratively and reset. */
void mulib_bk_tree_free(mulib_bk_tree_t *tree);
/** Budgeted variants: bound total DP cells, optional work initialized on every
 * return. Zero limit selects 10000000. Same mutation/callback contracts. */
int mulib_bk_tree_insert_bounded(mulib_bk_tree_t *, const char *, size_t, size_t *);
size_t mulib_bk_tree_search_bounded(const mulib_bk_tree_t *, const char *, size_t,
    size_t, mulib_bk_visit_fn, void *, size_t *);
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
| Header | [`include/mulib-bk-trees.h`](../include/mulib-bk-trees.h) |
| Implementation switch | `MULIB_IMPL_BK_TREES` |
| Repository-local includes | [mulib-levenshtein](mulib-levenshtein.md) |
| Complete example | [`examples/modules/bk-trees.c`](../examples/modules/bk-trees.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bk-trees.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
