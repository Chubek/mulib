# mulib-cgeom

Computational geometry on linalg vectors, with shared ray, AABB, sphere and
triangle descriptors. Define `MULIB_IMPL_CGEOM`, `MULIB_IMPL_SIMDMATH` and
`MULIB_IMPL_LINALG`; link `-lm`.

## Predicates and geometry

`orient2(a,b,c)` returns signed twice-area, positive for counterclockwise order.
`polygon_area` returns signed area, zero for fewer than three points, and NaN
for invalid/nonfinite inputs. NULL with zero count is valid. `polygon_contains`
uses even-odd inclusion and counts the boundary as inside; it returns -1 invalid,
0 outside, 1 inside. Degenerate polygons with fewer than three points are outside.
`segments_intersect` includes endpoint contact, collinear overlap and zero-length
segments; it uses the same -1/0/1 convention.

Double intermediates improve float-coordinate predicates but are **not adaptive
exact predicates**. Near-collinear/ill-conditioned configurations can be sensitive
to rounding. No global epsilon or implicit coordinate quantization is applied.

`closest_segment` clamps projection to the segment and returns its first endpoint
when it is degenerate. Inputs must be finite. `barycentric` computes weights of
the point's orthogonal projection onto a triangle using cross-product area ratios,
avoiding the subtraction of nearly equal Gram products for thin triangles. It does not
require the point to lie in the triangle plane. Degenerate/nonfinite triangles
fail. `bounds` requires a nonempty finite point array. These two checked output
functions return 1/0 and preserve output on failure.

Validity helpers accept finite vectors, ordered boxes (including flat boxes),
nonnegative finite sphere radii and nonzero finite ray directions.

## Convex hull and batches

`convex_hull` copies and sorts at most 1,048,576 points and constructs a monotone
chain. It removes duplicates and interior collinear points. The result is CCW,
starts at the lexicographically smallest point, and does not repeat the endpoint.
One unique point produces one output; a collinear set produces its two endpoints.
The input remains unchanged. Temporary storage is three point arrays and is
freed on every path. Input/output may overlap because copying precedes writing.

Pass NULL output and capacity zero to query the required count. Empty input
succeeds with count zero. Insufficient capacity returns 0, sets `*written` to
the required count and leaves output points unchanged. Invalid input/allocation
failure preserves both outputs. Otherwise success returns 1. `written` is required
and must not alias point storage. Capacity is in points, not bytes.

`distances_squared` uses four-lane SIMD batches and a scalar tail. Arrays must not
overlap; NULL arrays are permitted at count zero. It returns 1/0 for valid/invalid
array arguments. Numeric input follows IEEE arithmetic, including NaN/infinity
and float overflow; use the validity helpers when finite geometry is required.

See `examples/modules/cgeom.c` and `tests/test_graphics_math.c` for hull, boundary,
degeneracy, capacity and SIMD-tail checks.
