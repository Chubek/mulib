# mulib-vectgfx

Vector drawing API and a small absolute-coordinate language. Define
`MULIB_IMPL_VECTGFX`, `MULIB_IMPL_GLX`, `MULIB_IMPL_DRM`, `MULIB_IMPL_LINALG`,
`MULIB_IMPL_VEC`, `MULIB_IMPL_PARSEC`; link `-lm`.

## Paths

`create/destroy/clear` manage an opaque owned command vector. Builders return
1 on success, 0 with the existing path preserved on invalid state, allocation
failure or limits. Contours start with `move_to`; line, quadratic and cubic
Bezier commands require an open contour. `close` joins the last point to its
start and ends that contour. A subsequent command must begin another contour.
Moving again ends the preceding open contour without stroking a closing edge.

`catmull_to(previous,end,next)` appends a uniform Catmull-Rom segment from the
current point to end using its two neighboring points. Conversion uses cubic
controls `current+(end-previous)/6` and `end-(next-current)/6`.

`arc(center,rx,ry,start,sweep)` accepts positive radii and a signed radian sweep
up to a full turn (float representation of 2*pi accepted). It moves/lines to
the start, then appends cubic approximations in pieces of at most a quarter turn.
Zero sweep produces no visible curve. Positive angles follow GLX's downward-Y
coordinates, hence positive sweep is clockwise on screen. Arcs are cubic
approximations, not exact conics; flattening tolerance does not bound this initial
approximation error. Compound arc construction rolls back on failure.

Coordinates/control points must be finite and within +/-1,000,000. Radii are at
most 1,000,000. A path has at most 65536 stored commands. Descriptors are copied;
there is no retained caller geometry. Mutable paths need external synchronization.

## Language

```text
M 8 48 C 8 4 56 4 56 48 L 8 48 Z
M 20 25 Q 32 15 44 25 L 32 38 Z
```

Commands and exact argument counts:

| Command | Arguments |
| --- | --- |
| M / L | x y |
| Q | control-x control-y end-x end-y |
| C | control1-x control1-y control2-x control2-y end-x end-y |
| S | previous-x previous-y end-x end-y next-x next-y |
| A | center-x center-y radius-x radius-y start-angle sweep-angle |
| Z | none |

Commands are uppercase and absolute. Numbers are signed decimals with optional
fraction/exponent, at most 63 bytes, parsed independently of locale. Whitespace
is required between consecutive numbers, optional next to command letters.
There are no implicit repeated commands, commas, comments or SVG relative/smooth
command semantics. Parsec validates tokens and complete consumption. `parse`
borrows at most 64 KiB and returns an owned path or NULL with static diagnostic
and zero-based offset. Empty input (including NULL/zero) creates an empty path.

## Rasterization

Fill implicitly closes open contours and uses even-odd coverage, supporting
holes and self-intersections regardless of winding. Stroke uses the union of
segment capsules, giving round joins/caps and preventing double alpha blending
at overlaps. Curves flatten adaptively using de Casteljau subdivision; controls
must be within tolerance of the chord. Limits are recursion depth 16 and 262144
nonzero edges per rendering. Exceeding either returns 0 before drawing.

Tolerance must be finite in `(0,1000000]`; stroke width has the same range.
Filling allocates edges and scanline crossings; stroking allocates edges and a
one-byte-per-surface-pixel coverage mask. All temporary storage is released.
Allocation/validation failure leaves pixels untouched. Empty paths succeed.
Success returns 1. Rendering uses GLX pixel centers, source-over blending and
scissor, with no antialiasing, dashing or depth changes. The renderer is intended
for bounded software workloads; stroke work grows with segment bounding areas.

See `examples/modules/vectgfx.c` and `tests/test_graphics_render.c` for curves,
holes, round strokes, parser errors and alpha-union checks.
