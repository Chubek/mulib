# Pearson correlation

Define `MULIB_IMPL_PEARSON_SIMILARITY`; link `-lm`.
[Example](../examples/modules/pearson_similarity.c).

Computes centered covariance divided by the centered norms using scaled
coordinates and long-double sums. Result is clamped to [-1,1]. At least two
paired finite observations and nonconstant vectors are required; invalid or
undefined input returns NAN. Arrays are borrowed, not mutated. O(n) time,
constant storage. This does not compute p-values or establish causality;
near-constant input remains subject to floating-point resolution.
