# Bounded GLX paths and scanline filling

The additive, header-only C11 path renderer uses the shared `mgx_surface` and
straight `0xAARRGGBB` source-over compositing. It has no hardware or third-party
runtime dependency. Existing names and `mulib_glx_path_t` fields are retained.

## Example

```c
#include "mulib-glx-svg.h"
#include "mulib-glx-raster.h"

int main(void)
{
    static const char data[] = "M2 2H30V20H2Z M8 6V16H24V6Z";
    mulib_glx_path_t path = {0};
    mgx_surface surface = {0};
    int ok = mgx_surface_init(&surface, 32, 24);
    if (ok)
        ok = mulib_glx_svg_path_n(&path, data, sizeof data - 1);
    if (ok)
        ok = mulib_glx_raster_fill_checked(
            &surface, &path, 0xff4080c0, 0, 4);
    mulib_glx_path_free(&path);
    mgx_surface_free(&surface);
    return ok ? 0 : 1;
}
```

Compile with `cc -std=c11 -Ikernels example.c -lm -o example`.
The example renders headlessly into memory; it does not create a native window.

## Paths and curves

Zero-initialize an owning path, append with `mulib_glx_path_move`, `_line`,
`_quad`, `_cubic`, `_arc`, or `_close`, then call `mulib_glx_path_free`.
Do not copy an owning path. Read-only raster functions also accept borrowed
point/marker arrays with a valid count; never free or mutate a borrowed path
using the owning APIs.

Coordinates, including Bézier control points, must be finite and within
`[-100000000, 100000000]`. The default `MULIB_GLX_PATH_MAX_POINTS` is 1,048,576;
it can be lowered before including the headers, consistently in every
translation unit. Counts above `INT_MAX` are always rejected.

Quadratic and cubic curves are flattened by adaptive de Casteljau subdivision.
Both controls must lie within the requested distance of the **finite chord**
before a segment is emitted. This avoids losing collinear reversals and closed
loops. The default flatness is 0.25 coordinate units; `_quad_tol` and
`_cubic_tol` accept another positive finite tolerance. Flatten in device space,
or account for later transform magnification when choosing a tolerance.
Subdivision depth is limited to 24. Hitting that limit, the point limit, an
invalid coordinate, or an allocation failure returns 0; it does not pretend
that an under-resolved curve succeeded.

`_arc` is a circular center/radius arc in radians, not an SVG elliptical arc.
It includes both endpoints and connects to an open contour, with a 0.25-unit
sagitta bound. Large sweeps that exceed the point budget fail.

Failed curve/arc appends restore the logical point count and contour state.
Successfully grown allocations may remain owned by the path and are released
by `_free`. Appending one point likewise never exposes a half-appended point.

## Bounded SVG path-data subset

`mulib_glx_svg_path_n(path, bytes, length)` reads exactly the supplied span;
no NUL terminator is required. The default maximum span is 1,048,576 bytes
(`MULIB_GLX_SVG_MAX_BYTES`). Embedded NUL bytes are invalid. The original
NUL-terminated `mulib_glx_svg_path` remains available; its bounded scan accepts
strings shorter than that limit. Use `_n` to pass exactly the limit.

Supported commands are absolute and relative `M L H V Q C Z`, with repeated
coordinate groups and implicit lineto after moveto. A nonempty input must start
with moveto, even when appending to an existing path. A line after closepath
starts at the closed contour's origin. Empty/whitespace-only data is a no-op.

Numbers use locale-independent decimal syntax, including decimal fractions and
signed exponents. Tokens longer than 128 bytes, malformed exponents, nonfinite
values, hex notation, misplaced/repeated/trailing commas, and unsupported
commands are rejected. Very small decimals may round to zero. Decimal parsing
is not a correctly-rounded general-purpose numeric conversion API.

Any parse, geometry, allocation, or limit failure restores the entire append's
logical state, including `first` and `open`. Old points remain unchanged.

## Scanline filling

`mulib_glx_raster_fill_checked(surface, path, color, evenodd, samples)` returns
1 on success (including an empty or clipped-out fill), or 0 on invalid input or
allocation failure. All allocation and validation occur before painting, so a
failure does not partially paint. The original void `_fill` wrapper discards
that status for source compatibility.

Contours are implicitly closed. A nonzero `evenodd` selects even-odd fill;
zero selects nonzero winding. Crossings use half-open vertical intervals
`[ymin, ymax)` and horizontal spans `[left, right)`, retaining shared-edge
behavior. `samples` must be 1 through 8 and selects a regular square grid.

The fill builds a sorted edge table, maintains active edges for each subscanline,
sorts their crossings, and accumulates covered spans into a single pixel row.
It does **not** perform an edge walk for every pixel sample. Scratch storage is
O(path points + clipped row width). Overlapping contours and self-intersections
are resolved before compositing: every destination pixel is source-over blended
at most once per fill. Separate fill calls preserve the caller's painter order.
`_winding` and `_coverage` remain scalar reference/point-query APIs; the
antialias helper uses the scanline fill with a 4×4 grid.

## Validation

From the repository root:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -Wno-misleading-indentation -Ikernels tests/test-gfx-path.c \
  -lm -o /tmp/opencode/test-gfx-path
/tmp/opencode/test-gfx-path
cc -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -Wno-misleading-indentation -Ikernels tests/test-gfx-path-limits.c \
  -lm -o /tmp/opencode/gfx-path-limits
/tmp/opencode/gfx-path-limits
```

Repeat with `-fsanitize=address,undefined -fno-omit-frame-pointer -g` for memory
and undefined-behavior checks. `-Wno-misleading-indentation` accommodates compact
unchanged dependency headers.

Tests compare scanline pixels to scalar winding coverage on all eight grids,
both rules, clipped and unclipped compound paths, horizontal/degenerate edges,
large coordinates, self-intersections, adaptive curves, and deterministic
random polygons. They also check known holes, shared triangle edges, painter
order, padded stride, half-pixel coverage, non-NUL input, malformed/truncated
and mutated parser inputs, finite validation, curve accuracy, subdivision and
point limits, and injected allocation failures.

## Still unfinished

This is sampled coverage, not an exact analytic area rasterizer. Crossings are
sorted afresh on each subscanline; large active-edge sets can still be costly.
There is no path clip stack, compound-path tessellator, GPU path backend, or
full SVG/XML implementation. SVG `A/S/T`, transforms, styles, units, paint
servers and resource loading remain unsupported. Existing stroke caps/joins,
dash behavior, mesh, filter, paint, gradient, shape, and tessellation algorithms
are unchanged by this path improvement; in particular, separate translucent
stroke segments can still over-composite at their joins.
