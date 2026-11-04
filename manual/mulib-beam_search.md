# HMM state beam search

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
