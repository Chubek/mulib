# Viterbi decoding

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_VITERBI_DECODING`, `MULIB_IMPL_HIDDEN_MARKOV_CHAIN`,
`MULIB_IMPL_MARKOV_CHAIN`, `MULIB_IMPL_PROBABILITY`; link `-lm`.
[Example](../examples/modules/viterbi_decoding.c).

Decodes the highest joint-probability state path, not the sequence of marginal
posterior modes. Uses the shared discrete HMM and log-space scores. Ties select
lowest predecessor ID then lowest final state ID, not a separately sorted list
of whole paths. Path output has length slots; log probability is required and
must be separate from arrays. Outputs must not overlap borrowed model/observations.

Return 1 on success, 0 invalid/OOM/impossible, preserving outputs on failure.
Empty observations succeed with log probability zero and may have NULL path.
The beam variant retains only width best states per step (positive width).
Full width is exact. Runtime O(length*states²), backpointers O(length*states),
working scores/order O(states). Caller must bound total work/storage.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-viterbi_decoding.h`](../include/mulib-viterbi_decoding.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-viterbi_decoding.h
 * @brief Log-space Viterbi decoding with deterministic state ties.
 * Define MULIB_IMPL_VITERBI_DECODING and the hidden-Markov dependency switches.
 */
#ifndef MULIB_VITERBI_DECODING_H
#define MULIB_VITERBI_DECODING_H
#include "mulib-hidden_markov_chain.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Best state path and joint log probability. path has length slots, may be NULL
 * for length zero. Return 1 on success, 0 invalid/OOM/impossible; outputs unchanged.
 * Ties choose lowest predecessor and final state IDs. */
int mulib_viterbi_decode(const mulib_hidden_markov_chain_t *model, const size_t *observations,
                        size_t length, size_t *path, double *log_probability);
/** State-pruned decoding: retain best width states at each time step.
 * width>=states is exact. width must be positive. Used by beam_search. */
int mulib_viterbi_decode_beam(const mulib_hidden_markov_chain_t *model, const size_t *observations,
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
| Header | [`include/mulib-viterbi_decoding.h`](../include/mulib-viterbi_decoding.h) |
| Implementation switch | `MULIB_IMPL_VITERBI_DECODING` |
| Repository-local includes | [mulib-hidden_markov_chain](mulib-hidden_markov_chain.md) |
| Complete example | [`examples/modules/viterbi_decoding.c`](../examples/modules/viterbi_decoding.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/viterbi_decoding.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
