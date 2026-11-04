# Cosine similarity

Define `MULIB_IMPL_COSINE_SIMILARITY`; link `-lm`.
[Example](../examples/modules/cosine_similarity.c).

Computes a·b/(||a||||b||), clamped to [-1,1] for rounding. Each borrowed vector
is scaled by its maximum absolute coordinate before long-double accumulation,
avoiding squared-norm overflow. Two zero vectors (including empty) give 1;
exactly one zero vector gives 0, an explicit convention for otherwise undefined
angles. Nonfinite coordinates or NULL nonempty buffers give NAN.
Dense equal-length coordinates only, O(n) time, no allocations.
