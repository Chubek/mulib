# Pearson correlation

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_PEARSON_SIMILARITY`; link `-lm`.
[Example](../examples/modules/pearson_similarity.c).

Computes centered covariance divided by the centered norms using scaled
coordinates and long-double sums. Result is clamped to [-1,1]. At least two
paired finite observations and nonconstant vectors are required; invalid or
undefined input returns NAN. Arrays are borrowed, not mutated. O(n) time,
constant storage. This does not compute p-values or establish causality;
near-constant input remains subject to floating-point resolution.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-pearson_similarity.h`](../include/mulib-pearson_similarity.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-pearson_similarity.h
 * @brief Pearson correlation using scaled centered coordinates.
 * Define MULIB_IMPL_PEARSON_SIMILARITY; link libm.
 */
#ifndef MULIB_PEARSON_SIMILARITY_H
#define MULIB_PEARSON_SIMILARITY_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Return correlation in [-1,1]; NAN for n<2, constant vectors or invalid data. */
double mulib_pearson_similarity(const double *a, const double *b, size_t n);
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
| Header | [`include/mulib-pearson_similarity.h`](../include/mulib-pearson_similarity.h) |
| Implementation switch | `MULIB_IMPL_PEARSON_SIMILARITY` |
| Repository-local includes | None |
| Complete example | [`examples/modules/pearson_similarity.c`](../examples/modules/pearson_similarity.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/pearson_similarity.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
