# Finite Markov chains

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_MARKOV_CHAIN` and `MULIB_IMPL_PROBABILITY`; link `-lm`.
[Example](../examples/modules/markov_chain.c).

The model borrows a row-major n*n matrix, source state by destination state.
Rows and input distributions must be finite probabilities summing to one within
1e-9; dimensions are checked for overflow. Step multiplies a row distribution
by the matrix using O(n) temporary storage; output may alias input. Invalid/OOM
returns 0 without output changes. Dense validation/step take O(n²).

Fit estimates adjacent transitions of one state-ID sequence, with nonnegative
finite additive smoothing. It needs caller output n*n slots and O(n²) temporary
space. Unknown IDs, OOM, or a zero-mass row fail without mutation. Empty input
requires positive smoothing to succeed. Sequence boundaries are not inferred.
Sample takes a caller-supplied uniform number in [0,1), returns a destination
or SIZE_MAX on invalid input. It does not own RNG state or train a model.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-markov_chain.h`](../include/mulib-markov_chain.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-markov_chain.h
 * @brief Dense finite-state Markov chains and smoothed transition estimation.
 * Define MULIB_IMPL_MARKOV_CHAIN and MULIB_IMPL_PROBABILITY. Link libm.
 */
#ifndef MULIB_MARKOV_CHAIN_H
#define MULIB_MARKOV_CHAIN_H
#include "mulib-probability.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Borrowed row-major states*states transition matrix. */
typedef struct mulib_markov_chain { size_t states; const double *transition; } mulib_markov_chain_t;
/** Validate dimensions and stochastic rows (tolerance 1e-9). */
int mulib_markov_chain_valid(const mulib_markov_chain_t *chain);
/** Multiply a normalized row distribution by the matrix; output may alias input.
 * Return 0 on invalid/OOM without changing output. */
int mulib_markov_chain_step(const mulib_markov_chain_t *chain, const double *distribution, double *output);
/** Estimate from adjacent state IDs in one sequence with additive smoothing.
 * Output has states*states slots. Unobserved rows without smoothing fail.
 * Zero-length input is allowed; failure leaves output unchanged. */
int mulib_markov_chain_fit(size_t states, const size_t *sequence, size_t length,
                           double smoothing, double *transition);
/** Inverse-CDF sample from a state, given caller-supplied uniform value in [0,1).
 * SIZE_MAX on invalid input. No hidden RNG state. */
size_t mulib_markov_chain_sample(const mulib_markov_chain_t *chain, size_t state, double uniform);
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
| Header | [`include/mulib-markov_chain.h`](../include/mulib-markov_chain.h) |
| Implementation switch | `MULIB_IMPL_MARKOV_CHAIN` |
| Repository-local includes | [mulib-probability](mulib-probability.md) |
| Complete example | [`examples/modules/markov_chain.c`](../examples/modules/markov_chain.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/markov_chain.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
