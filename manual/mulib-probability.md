# Discrete probability

Define `MULIB_IMPL_PROBABILITY` in one C11 translation unit; link `-lm`.
See [example](../examples/modules/probability.c).

`mulib_probability_normalize` divides nonnegative finite weights by their
sum using maximum scaling to avoid sum overflow. It requires nonempty,
positive mass; failure returns 0 without mutation. All arrays are borrowed.
Validation requires each entry in [0,1] and a sum within the supplied finite,
nonnegative absolute tolerance. Empty distributions are invalid.

Entropy uses natural logarithms, with 0 log 0 = 0, and validates to 1e-9.
Invalid entropy inputs return NAN. `logsumexp` accepts negative/positive
infinity, returns -INFINITY for empty/all-negative-infinity input, and NAN for
NaN or a missing nonempty array. All operations are linear, allocation-free.
