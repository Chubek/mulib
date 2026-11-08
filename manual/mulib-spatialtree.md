# spatialtree

[Manuals](README.md) · [Library index](index.md)

Immutable median-balanced 2D k-d tree. `build` copies finite x/y/uint64-ID points
into an uninitialized destination; duplicate positions/IDs are allowed. Sorting
per level costs O(n log² n). Empty construction succeeds. `free` resets storage.

`query` searches inclusive rectangles, pruning by split coordinate. Callback
borrows a point; return nonzero to continue or zero to stop. Count includes the
stopping callback; SIZE_MAX means invalid input. Callback must not mutate/free
the tree. No radius/nearest-neighbor API, dynamic insertion or disk index.
Switch: `MULIB_IMPL_SPATIALTREE`. Example: `examples/modules/spatialtree.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-spatialtree.h`](../include/mulib-spatialtree.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-spatialtree.h
 * @brief Owned static balanced 2D k-d tree for inclusive rectangle searches.
 * Define MULIB_IMPL_SPATIALTREE once. Coordinates must be finite. */
#ifndef MULIB_SPATIALTREE_H
#define MULIB_SPATIALTREE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_spatialtree_point
  {
    double x, y;
    uint64_t id;
  } mulib_spatialtree_point_t;
  typedef struct mulib_spatialtree
  {
    mulib_spatialtree_point_t *points;
    size_t count;
  } mulib_spatialtree_t;
  typedef int (*mulib_spatialtree_visit_fn) (const mulib_spatialtree_point_t *,
                                             void *);
  /** @brief Copy/build into uninitialized output, 1 success. Duplicate
   * positions allowed. Median partition uses in-place sorting per level (O(n
   * log^2 n) construction). */
  int mulib_spatialtree_build (const mulib_spatialtree_point_t *points,
                               size_t count, mulib_spatialtree_t *out);
  /** @brief Visit inclusive rectangle. Callback nonzero continues, zero stops.
   * Returns count including stopping callback, SIZE_MAX invalid; do not mutate
   * tree. */
  size_t mulib_spatialtree_query (const mulib_spatialtree_t *tree, double xmin,
                                  double ymin, double xmax, double ymax,
                                  mulib_spatialtree_visit_fn visit,
                                  void *context);
  /** @brief Free/reset. */
  void mulib_spatialtree_free (mulib_spatialtree_t *tree);
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
| Header | [`include/mulib-spatialtree.h`](../include/mulib-spatialtree.h) |
| Implementation switch | `MULIB_IMPL_SPATIALTREE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/spatialtree.c`](../examples/modules/spatialtree.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/spatialtree.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
