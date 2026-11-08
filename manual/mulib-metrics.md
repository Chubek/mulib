# HTTP metrics

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_METRICS`. Zero-initialized single-threaded counters record
requests, bytes, caller-supplied duration microseconds and status classes 1..5.
Counters saturate at UINT64_MAX. No global registry/exporter/synchronization.
[Example](../examples/modules/metrics.c), [contracts](storage-server.md).

## Recording and aggregation

Zero-initialize `mulib_metrics_t`. `record()` accepts HTTP statuses 100..599,
byte counts, and a caller-measured duration in microseconds. It returns 1 and
increments requests, the relevant `status_class[status / 100]`, and the byte/time
totals. Index zero of `status_class` is unused. NULL state or an out-of-range
status returns 0 without changing counters.

Every counter saturates independently at `UINT64_MAX`; saturation still reports
success. Duration is a total, not an average, histogram, or percentile. There
is no clock read, allocation, reset function, export transport, or internal
synchronization. Reset by zeroing the structure while it is not being updated.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-metrics.h`](../include/mulib-metrics.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-metrics.h
 * @brief Saturating single-threaded HTTP counters and latency totals.
 * Define MULIB_IMPL_METRICS. Durations are caller-supplied microseconds. */
#ifndef MULIB_METRICS_H
#define MULIB_METRICS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_metrics
  {
    uint64_t requests, bytes_in, bytes_out, duration_us, status_class[6];
  } mulib_metrics_t;
  int mulib_metrics_record (mulib_metrics_t *metrics, unsigned status,
                            uint64_t input, uint64_t output,
                            uint64_t duration_us);
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
| Header | [`include/mulib-metrics.h`](../include/mulib-metrics.h) |
| Implementation switch | `MULIB_IMPL_METRICS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/metrics.c`](../examples/modules/metrics.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/metrics.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
