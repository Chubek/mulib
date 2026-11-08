# Euclidean distance

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_EUCLIDEAN_DISTANCE`; link `-lm`.
[Example](../examples/modules/euclidean_distance.c).

`mulib_euclidean_distance` computes the L2 distance of borrowed equal-length
double arrays using long-double differences and hypot accumulation, avoiding
intermediate squaring overflow. Nonfinite coordinates or missing nonempty
arrays give NAN. Empty vectors give 0; a result outside double range gives
INFINITY. O(n) time, no allocation. No squared-distance or sparse-vector API.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-euclidean_distance.h`](../include/mulib-euclidean_distance.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-euclidean_distance.h
 * @brief Euclidean distance with hypot accumulation.
 * Define MULIB_IMPL_EUCLIDEAN_DISTANCE; link libm.
 */
#ifndef MULIB_EUCLIDEAN_DISTANCE_H
#define MULIB_EUCLIDEAN_DISTANCE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Empty vectors give zero. Invalid/nonfinite input gives NAN; overflow gives INFINITY. */
double mulib_euclidean_distance(const double *a, const double *b, size_t n);
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
| Header | [`include/mulib-euclidean_distance.h`](../include/mulib-euclidean_distance.h) |
| Implementation switch | `MULIB_IMPL_EUCLIDEAN_DISTANCE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/euclidean_distance.c`](../examples/modules/euclidean_distance.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/euclidean_distance.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
