# Bayesian ranking

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_BAYSIAN_RANKING`; link `-lm`.
[Example](../examples/modules/baysian_ranking.c). Filename spelling is retained.

Scores are Beta-Bernoulli posterior means `(successes+alpha)/(trials+alpha+beta)`.
Finite counts satisfy 0 <= successes <= trials; alpha,beta must be positive.
Invalid scores return NAN. Fractional pseudocounts are accepted. Scaling avoids
overflow for large finite parameters. Scores are not confidence lower bounds.

`mulib_bayesian_rank` writes a permutation of original indices, descending by
finite score, with original-order ties. Input/output must not overlap; invalid
input fails before mutation. Empty arrays may be NULL. O(n²) time, no allocations.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-baysian_ranking.h`](../include/mulib-baysian_ranking.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-baysian_ranking.h
 * @brief Beta-Bernoulli posterior-mean scores and deterministic ranking.
 * Define MULIB_IMPL_BAYSIAN_RANKING. Historical filename retained.
 */
#ifndef MULIB_BAYSIAN_RANKING_H
#define MULIB_BAYSIAN_RANKING_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Score (successes+alpha)/(trials+alpha+beta); NAN for invalid input. */
double mulib_bayesian_rank_score(double successes, double trials, double alpha, double beta);
/** Rank finite scores descending, ties by original index; indices has n slots.
 * Output is unchanged on invalid input; scores and indices must not overlap. */
int mulib_bayesian_rank(const double *scores, size_t n, size_t *indices);
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
| Header | [`include/mulib-baysian_ranking.h`](../include/mulib-baysian_ranking.h) |
| Implementation switch | `MULIB_IMPL_BAYSIAN_RANKING` |
| Repository-local includes | None |
| Complete example | [`examples/modules/baysian_ranking.c`](../examples/modules/baysian_ranking.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/baysian_ranking.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
