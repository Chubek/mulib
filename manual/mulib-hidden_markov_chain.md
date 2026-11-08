# Discrete hidden Markov models

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_HIDDEN_MARKOV_CHAIN`, `MULIB_IMPL_MARKOV_CHAIN` and
`MULIB_IMPL_PROBABILITY`; link `-lm`.
[Example](../examples/modules/hidden_markov_chain.c).

Models borrow initial[states], transition[states*states] and
emission[states*symbols]. All rows are stochastic with tolerance 1e-9.
Emission at time zero uses initial state before any transition. Observations
are zero-based symbol IDs. Arrays remain caller-owned and immutable during calls.

Forward inference rescales every observation and accumulates log scale factors,
using long-double working values, O(length*states²) time and O(states) space.
Returns 1 with a log likelihood: zero for empty observations, -INFINITY for an
impossible sequence. Invalid input, dimension overflow or OOM returns 0 without
changing output. Extremely small products remain limited by platform floating
point. This module does not train HMMs or implement continuous emissions.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-hidden_markov_chain.h`](../include/mulib-hidden_markov_chain.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-hidden_markov_chain.h
 * @brief Discrete-emission hidden Markov models with scaled forward inference.
 * Define MULIB_IMPL_HIDDEN_MARKOV_CHAIN, MULIB_IMPL_MARKOV_CHAIN and
 * MULIB_IMPL_PROBABILITY. Link libm. Model arrays are borrowed, never modified.
 */
#ifndef MULIB_HIDDEN_MARKOV_CHAIN_H
#define MULIB_HIDDEN_MARKOV_CHAIN_H
#include "mulib-markov_chain.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_hidden_markov_chain {
  size_t states, symbols;
  const double *initial, *transition, *emission;
} mulib_hidden_markov_chain_t;
/** Validate initial distribution, transition rows and states*symbols emission rows. */
int mulib_hidden_markov_chain_valid(const mulib_hidden_markov_chain_t *model);
/** Log observation likelihood (scaled forward algorithm).
 * Empty observations give 0; impossible observations give -INFINITY, both success.
 * Return 0 on invalid/OOM, preserving output. */
int mulib_hidden_markov_chain_forward(const mulib_hidden_markov_chain_t *model,
                                     const size_t *observations, size_t length, double *log_likelihood);
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
| Header | [`include/mulib-hidden_markov_chain.h`](../include/mulib-hidden_markov_chain.h) |
| Implementation switch | `MULIB_IMPL_HIDDEN_MARKOV_CHAIN` |
| Repository-local includes | [mulib-markov_chain](mulib-markov_chain.md) |
| Complete example | [`examples/modules/hidden_markov_chain.c`](../examples/modules/hidden_markov_chain.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/hidden_markov_chain.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
