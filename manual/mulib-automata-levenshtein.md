# Streaming Levenshtein recognizer

Define `MULIB_IMPL_AUTOMATA_LEVENSHTEIN`.
[Example](../examples/modules/automata-levenshtein.c).

Creation copies the binary pattern and initializes the empty candidate state.
It returns NULL for invalid pointers, SIZE_MAX limit, dimension overflow or OOM.
`feed` consumes arbitrary byte chunks; chunk boundaries do not affect results.
It returns 0 on invalid input, without consuming it. `accepts` tests whether the
entire current candidate has edit distance <= limit. `reset` reuses storage.
Destroy the object once; NULL reset/destroy is harmless.

States are capped dynamic-programming rows, not a precompiled DFA graph.
O(pattern length) storage and per-byte work, no allocations during feed.
No Unicode normalization, transpositions, substring matching or tree traversal.
