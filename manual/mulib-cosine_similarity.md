# Cosine similarity

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_COSINE_SIMILARITY`; link `-lm`.
[Example](../examples/modules/cosine_similarity.c).

Computes a·b/(||a||||b||), clamped to [-1,1] for rounding. Each borrowed vector
is scaled by its maximum absolute coordinate before long-double accumulation,
avoiding squared-norm overflow. Two zero vectors (including empty) give 1;
exactly one zero vector gives 0, an explicit convention for otherwise undefined
angles. Nonfinite coordinates or NULL nonempty buffers give NAN.
Dense equal-length coordinates only, O(n) time, no allocations.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-cosine_similarity.h`](../include/mulib-cosine_similarity.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-cosine_similarity.h
 * @brief Scale-safe cosine similarity of finite double vectors.
 * Define MULIB_IMPL_COSINE_SIMILARITY; link libm.
 */
#ifndef MULIB_COSINE_SIMILARITY_H
#define MULIB_COSINE_SIMILARITY_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Two zero vectors (including empty) give 1, one zero vector gives 0.
 * Invalid pointers or nonfinite coordinates give NAN. */
double mulib_cosine_similarity(const double *a, const double *b, size_t n);
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
| Header | [`include/mulib-cosine_similarity.h`](../include/mulib-cosine_similarity.h) |
| Implementation switch | `MULIB_IMPL_COSINE_SIMILARITY` |
| Repository-local includes | None |
| Complete example | [`examples/modules/cosine_similarity.c`](../examples/modules/cosine_similarity.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/cosine_similarity.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
