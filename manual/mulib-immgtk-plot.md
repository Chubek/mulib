# mulib-immgtk-plot

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-immgtk-plot.h`  
**Implementation switch:** `MULIB_IMPL_IMMGTK_PLOT`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Engineering plot autoscaling bounds and logarithmic axis transformation helpers. Drawing, tick generation, linked cursors and virtualized multitrace plots are future work.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_IMMGTK_PLOT` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_IMMGTK_PLOT
#include "mulib-immgtk-plot.h"
```

## Focused regression test

`tests/test_immgtk_plot.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_immgtk_plot.c -lm -o /tmp/test_immgtk_plot
/tmp/test_immgtk_plot
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-immgtk-plot.h`](../include/mulib-immgtk-plot.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-immgtk-plot.h
 * @brief Engineering plot autoscaling bounds and logarithmic axis transformation helpers.
 * @details C11 single-header implementation; define MULIB_IMPL_IMMGTK_PLOT in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_IMMGTK_PLOT_H
#define MULIB_IMMGTK_PLOT_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
int mulib_immgtk_plot_bounds(const double *x, const double *y, size_t count, double *xmin,
                             double *xmax, double *ymin, double *ymax);
int mulib_immgtk_plot_log_transform(double x, double base, double *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMMGTK_PLOT_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-immgtk-plot.h`](../include/mulib-immgtk-plot.h) |
| Implementation switch | `MULIB_IMPL_IMMGTK_PLOT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/immgtk-plot.c`](../examples/modules/immgtk-plot.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/immgtk-plot.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
