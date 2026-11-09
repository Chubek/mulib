# mulib-immgtk-canvas

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-immgtk-canvas.h`  
**Implementation switch:** `MULIB_IMPL_IMMGTK_CANVAS`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

GUI-backend-neutral 2D pan/zoom transforms with cursor-centered zoom and reciprocal coordinate mapping. Does not create a native window or invoke ImGTK; intended as its reusable canvas math layer.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_IMMGTK_CANVAS` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_IMMGTK_CANVAS
#include "mulib-immgtk-canvas.h"
```

## Focused regression test

`tests/test_immgtk_canvas.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_immgtk_canvas.c -lm -o /tmp/test_immgtk_canvas
/tmp/test_immgtk_canvas
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-immgtk-canvas.h`](../include/mulib-immgtk-canvas.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-immgtk-canvas.h
 * @brief GUI-backend-neutral 2D pan/zoom transforms with cursor-centered zoom and reciprocal
 * coordinate mapping.
 * @details C11 single-header implementation; define MULIB_IMPL_IMMGTK_CANVAS in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_IMMGTK_CANVAS_H
#define MULIB_IMMGTK_CANVAS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_immgtk_canvas {
    double origin_x, origin_y, scale, offset_x, offset_y;
} mulib_immgtk_canvas_t;
int mulib_immgtk_canvas_world_to_screen(const mulib_immgtk_canvas_t *c, double x, double y,
                                        double *sx, double *sy);
int mulib_immgtk_canvas_screen_to_world(const mulib_immgtk_canvas_t *c, double x, double y,
                                        double *wx, double *wy);
int mulib_immgtk_canvas_zoom_at(mulib_immgtk_canvas_t *c, double screen_x, double screen_y,
                                double multiplier);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMMGTK_CANVAS_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-immgtk-canvas.h`](../include/mulib-immgtk-canvas.h) |
| Implementation switch | `MULIB_IMPL_IMMGTK_CANVAS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/immgtk-canvas.c`](../examples/modules/immgtk-canvas.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/immgtk-canvas.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
