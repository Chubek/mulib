# mulib-immgtk-drawlist

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-immgtk-drawlist.h`  
**Implementation switch:** `MULIB_IMPL_IMMGTK_DRAWLIST`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Backend-neutral owned draw-command array for lines, rectangles and circles; renderer retains clipping and rasterization responsibilities. Can be consumed by native MuLIB GLX or software renderer.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_IMMGTK_DRAWLIST` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_IMMGTK_DRAWLIST
#include "mulib-immgtk-drawlist.h"
```

## Focused regression test

`tests/test_immgtk_drawlist.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_immgtk_drawlist.c -lm -o /tmp/test_immgtk_drawlist
/tmp/test_immgtk_drawlist
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-immgtk-drawlist.h`](../include/mulib-immgtk-drawlist.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-immgtk-drawlist.h
 * @brief Backend-neutral owned draw-command array for lines, rectangles and circles; renderer
 * retains clipping and rasterization responsibilities.
 * @details C11 single-header implementation; define MULIB_IMPL_IMMGTK_DRAWLIST in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_IMMGTK_DRAWLIST_H
#define MULIB_IMMGTK_DRAWLIST_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_immgtk_primitive {
    MULIB_IMMGTK_LINE = 1,
    MULIB_IMMGTK_RECT = 2,
    MULIB_IMMGTK_CIRCLE = 3
} mulib_immgtk_primitive_t;
typedef struct mulib_immgtk_drawcmd {
    mulib_immgtk_primitive_t type;
    double x1, y1, x2, y2, width;
    uint32_t rgba;
} mulib_immgtk_drawcmd_t;
typedef struct mulib_immgtk_drawlist {
    mulib_immgtk_drawcmd_t *cmds;
    size_t count, cap;
} mulib_immgtk_drawlist_t;
int mulib_immgtk_drawlist_push(mulib_immgtk_drawlist_t *d, mulib_immgtk_drawcmd_t command);
void mulib_immgtk_drawlist_clear(mulib_immgtk_drawlist_t *d);
void mulib_immgtk_drawlist_free(mulib_immgtk_drawlist_t *d);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMMGTK_DRAWLIST_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-immgtk-drawlist.h`](../include/mulib-immgtk-drawlist.h) |
| Implementation switch | `MULIB_IMPL_IMMGTK_DRAWLIST` |
| Repository-local includes | None |
| Complete example | [`examples/modules/immgtk-drawlist.c`](../examples/modules/immgtk-drawlist.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/immgtk-drawlist.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
