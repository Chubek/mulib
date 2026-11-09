# mulib-asic-sta

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-asic-sta.h`  
**Implementation switch:** `MULIB_IMPL_ASIC_STA`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Arrival-time max-plus propagation on a topologically ordered timing DAG with up to eight predecessors per node, plus slack against a common deadline. Failed propagation preserves arrivals and critical-path output. No clock, slew, Liberty or multi-corner analysis.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_ASIC_STA` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_ASIC_STA
#include "mulib-asic-sta.h"
```

## Focused regression test

`tests/test_asic_sta.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_asic_sta.c -lm -o /tmp/test_asic_sta
/tmp/test_asic_sta
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-asic-sta.h`](../include/mulib-asic-sta.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-asic-sta.h
 * @brief Arrival-time max-plus propagation on a topologically ordered timing DAG with up to eight
 * predecessors per node, plus slack against a common deadline.
 * @details C11 single-header implementation; define MULIB_IMPL_ASIC_STA in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_ASIC_STA_H
#define MULIB_ASIC_STA_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_asic_timing_node {
    double delay, arrival;
    uint16_t pred[8];
    uint8_t pred_count;
} mulib_asic_timing_node_t;
int mulib_asic_sta_propagate(mulib_asic_timing_node_t *nodes, size_t count, double *critical_path);
int mulib_asic_sta_slack(const mulib_asic_timing_node_t *nodes, size_t count, double deadline,
                         double *slack);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ASIC_STA_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-asic-sta.h`](../include/mulib-asic-sta.h) |
| Implementation switch | `MULIB_IMPL_ASIC_STA` |
| Repository-local includes | None |
| Complete example | [`examples/modules/asic-sta.c`](../examples/modules/asic-sta.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/asic-sta.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
