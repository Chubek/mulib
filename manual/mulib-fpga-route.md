# mulib-fpga-route

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-fpga-route.h`  
**Implementation switch:** `MULIB_IMPL_FPGA_ROUTE`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Shortest Manhattan four-neighbour path on a rectangular 2D obstacle grid, BFS with deterministic neighbour ordering. Intended as an FPGA routing primitive; no negotiated congestion, switch boxes or timing models.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Maximum 65536 grid cells; path elements are uint16_t and therefore this profile is suitable only for grids <=65536 cells. Larger grids must be rejected until a wider path API exists.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_FPGA_ROUTE` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_FPGA_ROUTE
#include "mulib-fpga-route.h"
```

## Focused regression test

`tests/test_fpga_route.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_fpga_route.c -lm -o /tmp/test_fpga_route
/tmp/test_fpga_route
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-fpga-route.h`](../include/mulib-fpga-route.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-fpga-route.h
 * @brief Shortest Manhattan four-neighbour path on a rectangular 2D obstacle grid, BFS with
 * deterministic neighbour ordering.
 * @details C11 single-header implementation; define MULIB_IMPL_FPGA_ROUTE in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_FPGA_ROUTE_H
#define MULIB_FPGA_ROUTE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_fpga_grid {
    uint16_t width, height;
    const uint8_t *blocked;
} mulib_fpga_grid_t;
int mulib_fpga_route_bfs(mulib_fpga_grid_t grid, uint16_t sx, uint16_t sy, uint16_t gx, uint16_t gy,
                         uint16_t *path, size_t capacity, size_t *out_length);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FPGA_ROUTE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-fpga-route.h`](../include/mulib-fpga-route.h) |
| Implementation switch | `MULIB_IMPL_FPGA_ROUTE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/fpga-route.c`](../examples/modules/fpga-route.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/fpga-route.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
