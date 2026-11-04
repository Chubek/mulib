# Bayesian ranking

Define `MULIB_IMPL_BAYSIAN_RANKING`; link `-lm`.
[Example](../examples/modules/baysian_ranking.c). Filename spelling is retained.

Scores are Beta-Bernoulli posterior means `(successes+alpha)/(trials+alpha+beta)`.
Finite counts satisfy 0 <= successes <= trials; alpha,beta must be positive.
Invalid scores return NAN. Fractional pseudocounts are accepted. Scaling avoids
overflow for large finite parameters. Scores are not confidence lower bounds.

`mulib_bayesian_rank` writes a permutation of original indices, descending by
finite score, with original-order ties. Input/output must not overlap; invalid
input fails before mutation. Empty arrays may be NULL. O(n²) time, no allocations.
