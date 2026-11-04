# Byte edit distance

Define `MULIB_IMPL_EDIT_DISTANCE` and `MULIB_IMPL_LEVENSHTEIN`.
[Example](../examples/modules/edit_distance.c).

The C-string API reuses MuLIB Levenshtein. The `_n` API compares binary buffers,
including embedded NUL, with unit insertion/deletion/substitution costs.
Matching costs zero; no transpositions or Unicode grapheme segmentation.
NULL buffers require zero length in `_n`; C-string inputs cannot be NULL.
SIZE_MAX signals invalid input, dimension overflow or allocation failure.
Arrays are borrowed. `_n` uses O(min(n,m)) temporary storage and O(n*m) time;
empty-side results require no allocation. Caller must bound computational work.
