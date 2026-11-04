# Euclidean distance

Define `MULIB_IMPL_EUCLIDEAN_DISTANCE`; link `-lm`.
[Example](../examples/modules/euclidean_distance.c).

`mulib_euclidean_distance` computes the L2 distance of borrowed equal-length
double arrays using long-double differences and hypot accumulation, avoiding
intermediate squaring overflow. Nonfinite coordinates or missing nonempty
arrays give NAN. Empty vectors give 0; a result outside double range gives
INFINITY. O(n) time, no allocation. No squared-distance or sparse-vector API.
