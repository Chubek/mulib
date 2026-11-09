# mulib-hls-schedule

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-hls-schedule.h`  
**Implementation switch:** `MULIB_IMPL_HLS_SCHEDULE`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Resource-unconstrained ASAP scheduling of a topologically sorted operation DAG with integral cycle latencies and up to eight dependencies. No modulo scheduling, binding or port constraints.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_HLS_SCHEDULE` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_HLS_SCHEDULE
#include "mulib-hls-schedule.h"
```

## Focused regression test

`tests/test_hls_schedule.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_hls_schedule.c -lm -o /tmp/test_hls_schedule
/tmp/test_hls_schedule
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-hls-schedule.h`](../include/mulib-hls-schedule.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-hls-schedule.h
 * @brief Resource-unconstrained ASAP scheduling of a topologically sorted operation DAG with
 * integral cycle latencies and up to eight dependencies.
 * @details C11 single-header implementation; define MULIB_IMPL_HLS_SCHEDULE in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_HLS_SCHEDULE_H
#define MULIB_HLS_SCHEDULE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_hls_operation {
    uint32_t latency;
    uint16_t deps[8];
    uint8_t dep_count;
    uint32_t start;
} mulib_hls_operation_t;
int mulib_hls_schedule_asap(mulib_hls_operation_t *ops, size_t count, uint32_t *makespan);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HLS_SCHEDULE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-hls-schedule.h`](../include/mulib-hls-schedule.h) |
| Implementation switch | `MULIB_IMPL_HLS_SCHEDULE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/hls-schedule.c`](../examples/modules/hls-schedule.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/hls-schedule.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
