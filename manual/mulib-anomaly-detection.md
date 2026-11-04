# Streaming z-score anomalies

Define `MULIB_IMPL_ANOMALY_DETECTION` and `MULIB_IMPL_STATS`; link `-lm`.
[Example](../examples/modules/anomaly-detection.c).

Initialize a `mulib_stats_t` baseline with `mulib_stats_init`. Train accepts
finite values and rejects count/numerical overflow without mutation. Z-score
uses absolute deviation divided by sample standard deviation. It does not add
the queried value to the baseline. Fewer than two training samples, invalid
baseline or nonfinite query gives NAN. Zero variance gives zero at the mean,
INFINITY for any different query.

Detect returns 1 when z strictly exceeds a finite nonnegative threshold,
0 otherwise, -1 for invalid inputs/undefined score. No allocations; constant
work/storage. This is a univariate heuristic, not a robust/multivariate detector;
outliers in training and distribution drift can compromise the baseline.
