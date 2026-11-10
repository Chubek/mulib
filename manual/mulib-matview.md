# matview — incremental count and sum

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_MATVIEW`; link `-lm`.

Zero state is empty. Insert/delete return 1 on success, 0 for invalid/nonfinite
input, count overflow/underflow or nonfinite resulting sum. Failure preserves
the view. Delete subtracts a caller-supplied existing value: the view does not
retain members and cannot verify membership. Removing the last row resets sum
to zero to avoid residual rounding drift. Average returns sum/rows or NaN for
empty/NULL views. Operations are O(1), allocation-free, with ordinary floating
rounding rather than compensated or exact arithmetic.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-matview.h`](../include/mulib-matview.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Incrementally maintained count/sum materialized numeric aggregate view. C11
 * single-header; define MULIB_IMPL_MATVIEW once. */
#ifndef MULIB_MATVIEW_H
#define MULIB_MATVIEW_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t rows;
    double sum;
  } mulib_matview_t;
  int mulib_matview_insert (mulib_matview_t *v, double value);
  int mulib_matview_delete (mulib_matview_t *v, double value);
  double mulib_matview_avg (const mulib_matview_t *v);
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
| Header | [`include/mulib-matview.h`](../include/mulib-matview.h) |
| Implementation switch | `MULIB_IMPL_MATVIEW` |
| Repository-local includes | None |
| Complete example | [`examples/modules/matview.c`](../examples/modules/matview.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/matview.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
