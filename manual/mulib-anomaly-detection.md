# Streaming z-score anomalies

[Manuals](README.md) · [Library index](index.md)

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

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-anomaly-detection.h`](../include/mulib-anomaly-detection.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-anomaly-detection.h
 * @brief Univariate streaming z-score anomaly detection over Welford statistics.
 * Define MULIB_IMPL_ANOMALY_DETECTION and MULIB_IMPL_STATS; link libm.
 */
#ifndef MULIB_ANOMALY_DETECTION_H
#define MULIB_ANOMALY_DETECTION_H
#include "mulib-stats.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Absolute z score against an existing baseline; does not train on the query.
 * NAN before two samples or for invalid data. Zero variance: 0 at mean, INFINITY otherwise. */
double mulib_anomaly_zscore(const mulib_stats_t *baseline, double value);
/** Return 1 if z>threshold, 0 otherwise, -1 for invalid baseline/value/threshold. */
int mulib_anomaly_detect(const mulib_stats_t *baseline, double value, double threshold);
/** Add a finite training value, rejecting overflow without changing the baseline. */
int mulib_anomaly_train(mulib_stats_t *baseline, double value);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-anomaly-detection.h`](../include/mulib-anomaly-detection.h) |
| Implementation switch | `MULIB_IMPL_ANOMALY_DETECTION` |
| Repository-local includes | [mulib-stats](mulib-stats.md) |
| Complete example | [`examples/modules/anomaly-detection.c`](../examples/modules/anomaly-detection.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/anomaly-detection.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
