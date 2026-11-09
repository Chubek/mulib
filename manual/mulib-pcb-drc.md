# mulib-pcb-drc

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-pcb-drc.h`  
**Implementation switch:** `MULIB_IMPL_PCB_DRC`  
**Dependencies:** `pcb-geometry`.

## Purpose and supported profile

All-pairs straight-segment copper clearance checks for distinct nets. O(n²), reports violations up to caller capacity, always returns full hit count. No layers, pads, vias, zones or mask geometry.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_PCB_DRC` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_PCB_DRC
#include "mulib-pcb-drc.h"
```

## Focused regression test

`tests/test_pcb_drc.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_pcb_drc.c -lm -o /tmp/test_pcb_drc
/tmp/test_pcb_drc
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-pcb-drc.h`](../include/mulib-pcb-drc.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-pcb-drc.h
 * @brief All-pairs straight-segment copper clearance checks for distinct nets.
 * @details C11 single-header implementation; define MULIB_IMPL_PCB_DRC in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_PCB_DRC_H
#define MULIB_PCB_DRC_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-pcb-geometry.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_pcb_drc_violation {
    size_t first, second;
    double actual, required;
} mulib_pcb_drc_violation_t;
size_t mulib_pcb_drc_segments(const mulib_pcb_segment_t *tracks, size_t count, double clearance,
                              mulib_pcb_drc_violation_t *out, size_t capacity);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PCB_DRC_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-pcb-drc.h`](../include/mulib-pcb-drc.h) |
| Implementation switch | `MULIB_IMPL_PCB_DRC` |
| Repository-local includes | [mulib-pcb-geometry](mulib-pcb-geometry.md) |
| Complete example | [`examples/modules/pcb-drc.c`](../examples/modules/pcb-drc.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/pcb-drc.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
