# Bayesian updating

Define `MULIB_IMPL_BAYESIAN_METHODS` and `MULIB_IMPL_PROBABILITY`; link `-lm`.
[Example](../examples/modules/bayesian_methods.c).

`mulib_bayesian_update` normalizes log(prior)+log_likelihood with log-sum-exp.
Prior must be a nonempty distribution (tolerance 1e-9), log likelihoods <= 0,
including -INFINITY for impossible evidence. This API is for discrete event
likelihoods, not probability densities greater than one. Zero total evidence,
invalid inputs, size overflow and allocation failure return 0.

Output is caller-owned with n slots and may alias prior, but not likelihoods.
Optional log_evidence must be separate from arrays. Failure preserves outputs.
Temporary storage is O(n), time O(n). This is finite-hypothesis updating, not
a general inference/sampling framework.
