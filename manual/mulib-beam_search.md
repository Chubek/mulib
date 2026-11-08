# HMM state beam search

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_BEAM_SEARCH`, `MULIB_IMPL_VITERBI_DECODING`,
`MULIB_IMPL_HIDDEN_MARKOV_CHAIN`, `MULIB_IMPL_MARKOV_CHAIN` and
`MULIB_IMPL_PROBABILITY`; link `-lm`.
[Example](../examples/modules/beam_search.c).

Uses the shared trellis decoder: retain only the width best scoring states
after each emission, with deterministic low-state-ID ties. Width must be
positive. Width >= states gives exact Viterbi. Narrow beams are approximate
and can discard the sole eventually feasible path; zero return means invalid,
OOM or no surviving complete path. Outputs remain unchanged on failure.

Output ownership, empty-input handling and numerical conventions follow
[Viterbi](mulib-viterbi_decoding.md). This is state-pruned decoding, not an
N-best history beam, generic callback graph search or language model generator.
It retains the full backpointer table and dense transition computation.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-beam_search.h`](../include/mulib-beam_search.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-beam_search.h
 * @brief State-pruned beam search over discrete HMM trellises.
 * Define MULIB_IMPL_BEAM_SEARCH and VITERBI_DECODING plus its dependencies.
 * This retains states, not multiple histories per state; it is not an N-best API.
 */
#ifndef MULIB_BEAM_SEARCH_H
#define MULIB_BEAM_SEARCH_H
#include "mulib-viterbi_decoding.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Approximate best path; width>=states gives Viterbi. Return 0 for invalid,
 * OOM or no surviving complete path, preserving outputs. No optimality promise
 * when width<states; ties and ownership follow mulib_viterbi_decode. */
int mulib_beam_search(const mulib_hidden_markov_chain_t *model, const size_t *observations,
                      size_t length, size_t width, size_t *path, double *log_probability);
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
| Header | [`include/mulib-beam_search.h`](../include/mulib-beam_search.h) |
| Implementation switch | `MULIB_IMPL_BEAM_SEARCH` |
| Repository-local includes | [mulib-viterbi_decoding](mulib-viterbi_decoding.md) |
| Complete example | [`examples/modules/beam_search.c`](../examples/modules/beam_search.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/beam_search.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
