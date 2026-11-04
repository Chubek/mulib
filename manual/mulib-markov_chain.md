# Finite Markov chains

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
