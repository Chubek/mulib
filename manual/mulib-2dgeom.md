# mulib-2dgeom

[Manuals](README.md) · [Library index](index.md)

Computational 2D vector-path geometry, not a renderer. Define
`MULIB_IMPL_2DGEOM`, `MULIB_IMPL_GEOMETRY`, `MULIB_IMPL_VEC` and
`MULIB_IMPL_PARSEC` in one C11 translation unit; link libm. Points reuse
`mulib_geometry_point_t` with double coordinates.

## Curves

`mulib_2dgeom_curve_t` has degree 1/2/3 and respectively 2/3/4 Bezier control
points. Coordinates must be finite and have absolute magnitude <=1e100.
Parameters t must be finite and in [0,1]. APIs return 1 success, 0 on invalid
input, numerical/domain error, allocation failure or subdivision/work limit;
outputs are preserved on failure.

- `evaluate`: de Casteljau evaluation.
- `derivatives`: analytic first and second derivative curves (line second
  derivative is zero).
- `tangent`: unit first derivative; stationary points fail rather than fabricate
  a direction.
- `curvature`: signed `(x'y''-y'x'') / |P'|^3`; stationary or unrepresentable
  values fail.
- `split`: de Casteljau control polygons at arbitrary t, with exact shared
  split point up to floating rounding. End parameters give degenerate pieces.
  Outputs must be distinct and may alias input.
- `bounds`: endpoint and analytic derivative-extremum bounds, with normalized
  quadratic coefficients and stable root evaluation; not just control-box bounds.
- `length`: adaptive chord/control-polygon bounds; half their sum approximates
  length within the requested absolute tolerance when successful. Error budget
  is divided among subdivisions.
- `flatten`: adaptive control-hull-to-chord-segment distance, returning owned
  ordered points including both endpoints. The criterion also handles collinear
  overshoot and degenerate chords. Output must be zero-initialized/previous;
  release with `polyline_free`.

Subdivision is iterative, at most 32 levels and 1000000 work/leaves. An
unachievable tolerance fails rather than silently returning a coarse curve.
Predicates/bounds are floating-point, not adaptive exact geometry.

## Paths and DSL

Zero-initialize `mulib_2dgeom_path_t`. `path_append(curve, move)` copies a segment;
nonzero move starts a new contour, otherwise starts must match the preceding
endpoint. `path_at` borrows read-only segments until mutation; `path_count`
reports their number. The cap is 65536 segments. `path_free` accepts NULL.

`path_parse` compiles this **absolute-coordinate DSL through parsec**:

```text
M 0 0
L 10 0
Q 12 2 10 4
C 8 6 2 6 0 4
Z
```

M moves, L draws a line, Q a quadratic, C a cubic; Z adds an explicit straight
closing edge to the current contour start. A move is required before drawing.
Whitespace/commas separate parameters, numbers support signs, decimals and
exponents, and empty input is valid. A move without following drawing has no
stored segment. Moves and post-Z drawing start new contours. There are no
relative commands, implied repeated commands, arcs, comments or full SVG/XML
syntax. Numeric conversion uses the C numeric locale. NUL/trailing junk,
nonfinite numbers and missing parameters fail.

Parsing is entire-input, source is not retained, input cap 16 MiB, parsec depth
256/steps 10000000/captures 524288 plus its default work bound. It atomically
replaces an initialized output path; optional error holds a static message,
byte offset and one-based byte line/column. On success error is cleared.

`path_bounds` combines exact per-curve extrema (empty path has no box).
`path_transform` atomically applies affine `[a,b,c,d,e,f]` mapping
`(x,y)->(a*x+c*y+e,b*x+d*y+f)` to all control points.
`path_split(index,t,left,right)` returns open pieces including the two local
curve pieces. Outputs must be initialized/distinct/not input aliases.
`path_area` integrates the Bezier polynomial using degree-sufficient Gaussian
quadrature and analytically closes each contour by a line; signs distinguish
orientation, and self-intersections contribute algebraic signed area.

This library does not implement curve boolean operations, robust arrangement
construction, stroke expansion, clipping, point-in-fill, path intersection
enumeration or a drawing GUI. Existing geometry modules supply complementary
line/polygon operations. See the
[example](../examples/modules/2dgeom.c) and `tests/test_2dgeom.c` for subdivision
identities, analytic curvature/length, exact extrema, transforms and DSL errors.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-2dgeom.h`](../include/mulib-2dgeom.h) |
| Implementation switch | `MULIB_IMPL_2DGEOM` |
| Repository-local includes | [mulib-geometry](mulib-geometry.md), [mulib-parsec](mulib-parsec.md), [mulib-vec](mulib-vec.md) |
| Complete example | [`examples/modules/2dgeom.c`](../examples/modules/2dgeom.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/2dgeom.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
