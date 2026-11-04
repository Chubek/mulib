# Viterbi decoding

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
