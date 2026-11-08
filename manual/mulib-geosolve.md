# mulib-geosolve

[Manuals](README.md) · [Library index](index.md)

General-purpose **local geometric constraint solving** in two and three
dimensions, with analytic intersection/construction helpers. Custom residual
callbacks extend the same solver to curves, surfaces, mechanisms, fitting and
other application-specific geometry without adding new solver types.

This is not an exact/symbolic geometry engine, CAD topology kernel, global
feasibility proof, or enumerator of every solution branch. It complements
[`geometry`](mulib-geometry.md), [`cgeom`](mulib-cgeom.md) and
[`collision`](mulib-collision.md); use those existing modules for polygon/hull,
barycentric, bounding-volume and ray/collision queries.

## Integration

Define these switches before including any dependency headers, in exactly one
implementation translation unit:

```c
#define MULIB_IMPL_GEOSOLVE
#define MULIB_IMPL_GEOMETRY
#define MULIB_IMPL_CGEOM
#define MULIB_IMPL_LINALG
#define MULIB_IMPL_SIMDMATH
#define MULIB_IMPL_ALLOC
#define MULIB_IMPL_PORT
#include "mulib-geosolve.h"
```

Other translation units include the header without switches. No dependency's
implementation is enabled implicitly. ISO C11; public declarations support
C++17 linkage. Link `-lm` on platforms that require it. There are no mandatory
third-party dependencies.

## Problem model and ownership

`mulib_geosolve_problem_t` borrows four arrays:

- `values[variable_count]`: finite double scalar parameters. A point is simply
  two or three indices into this array; radii and other parameters use the same
  storage. Constraints may share or repeat indices. Nothing requires contiguous
  point coordinates in the array.
- `fixed[variable_count]`: nonzero bytes lock parameters bit-for-bit. NULL means
  every parameter is free. Anchoring points or coordinates removes rigid-motion
  freedoms without introducing artificial high-weight equations.
- `constraints[constraint_count]`: descriptors, usually initialized with
  `mulib_geosolve_constraint(kind)`, which zeroes all fields and sets weight 1.
  Set the required coordinate indices and target value. Built-ins infer their
  row counts; only CUSTOM uses the callback, user and residual_count fields.
- Optional allocator callbacks in the options are borrowed as well.

NULL values/constraints are allowed only with the corresponding zero count.
Empty problems succeed. A problem with no constraints leaves values untouched
and reports all free variables as unconstrained. Constant custom equations with
zero variables are supported. All initial values, targets and weights must be
finite; weights must be strictly positive. Remove a constraint to disable it.

The arrays, options, result and callback context must not overlap. The solver
retains nothing and frees all temporary storage before returning. It allocates
one double workspace and, when variables are free, one index workspace, using
`mulib-alloc`. An allocate/deallocate-only allocator is sufficient; realloc is
not needed. Distinct problems may be solved concurrently if their allocators
and callbacks are independently thread-safe.

### Example

```c
/* Anchored A=(0,0), B=(3,0), unknown C initially (1,2). */
double x[] = {0, 0, 3, 0, 1, 2};
unsigned char fixed[] = {1, 1, 1, 1, 0, 0};
mulib_geosolve_constraint_t c[2];
c[0] = mulib_geosolve_constraint(MULIB_GEOSOLVE_DISTANCE2);
c[0].indices[0] = 0; c[0].indices[1] = 1;
c[0].indices[2] = 4; c[0].indices[3] = 5;
c[0].value = 4;
c[1] = c[0];
c[1].indices[0] = 2; c[1].indices[1] = 3;
c[1].value = 5;
mulib_geosolve_problem_t p = {x, 6, fixed, c, 2};
mulib_geosolve_result_t result;
mulib_geosolve_status_t status = mulib_geosolve_solve(&p, NULL, &result);
/* SOLVED gives C approximately (0,4), rank 2, degrees_of_freedom 0. */
```

The mirrored solution `(0,-4)` is also valid; the initial guess selects a local
branch. See [`examples/modules/geosolve.c`](../examples/modules/geosolve.c) for
a runnable example that constructs the solved triangle's circumcircle.

## Built-in constraint reference

Enum names below omit `MULIB_GEOSOLVE_`. Each point in the index order expands
to `x,y` or `x,y,z`. Thus `DISTANCE3` uses six indices in the order
`ax,ay,az,bx,by,bz`. Let `u=(b-a)/|b-a|` and `v=(d-c)/|d-c|`.
Each listed residual is multiplied by `sqrt(weight)` during solving.

| Kind | Index order | Rows | Unweighted residual |
| --- | --- | --- | --- |
| VALUE | x | 1 | x - value |
| EQUAL | x,y | 1 | x - y - value |
| DISTANCE2 / DISTANCE3 | a,b | 1 | distance(a,b) - value |
| COINCIDENT2 / COINCIDENT3 | a,b | 2 / 3 | a - b, componentwise |
| MIDPOINT2 / MIDPOINT3 | p,a,b | 2 / 3 | p - (a+b)/2 |
| POINT_ON_LINE2 | p,a,b | 1 | signed perpendicular distance to infinite line ab |
| POINT_ON_PLANE3 | p,a,b,c | 1 | signed distance to plane abc, oriented by (b-a) cross (c-a) |
| PARALLEL2 | a,b,c,d | 1 | u.x*v.y - u.y*v.x |
| PARALLEL3 | a,b,c,d | 3 | u cross v |
| PERPENDICULAR2 / PERPENDICULAR3 | a,b,c,d | 1 | u dot v |
| ANGLE2 | a,b,c,d | 1 | signed angle from ab to cd minus value, wrapped to [-pi,pi] |
| ANGLE3 | a,b,c,d | 1 | unsigned angle between ab and cd minus value |
| EQUAL_LENGTH2 / EQUAL_LENGTH3 | a,b,c,d | 1 | distance(a,b) - distance(c,d) - value |
| POINT_ON_CIRCLE2 | p,center,radius | 1 | distance(p,center) - radius |
| POINT_ON_SPHERE3 | p,center,radius | 1 | distance(p,center) - radius |
| TANGENT_EXTERNAL2 | center1,radius1,center2,radius2 | 1 | distance(center1,center2) - radius1 - radius2 |
| TANGENT_INTERNAL2 | center1,radius1,center2,radius2 | 1 | distance(center1,center2) - abs(radius1-radius2) |
| TRIANGLE_AREA2 | a,b,c | 1 | signed area(a,b,c) - value |

All angles are radians. ANGLE2 targets must lie in `[-pi,pi]`, ANGLE3 targets
in `[0,pi]`, and distance targets must be nonnegative. `value` is ignored for
coincidence, midpoint, incidence, parallelism, perpendicularity and tangency.
Set it to zero in those descriptors. Horizontal/vertical lines use EQUAL on
the endpoints' y/x indices. Fix a coordinate either with the fixed mask or a
VALUE equation. Variable radii must be nonnegative whenever evaluated.

Parallel constraints accept both aligned and anti-aligned directions. Tangency
equations also allow degenerate point circles and coincident equal circles;
applications requiring distinct positive circles must enforce that domain.
Triangle area is positive for CCW order. Point-on-line/plane constraints apply
to the infinite supporting line/plane, not a finite segment/triangle.

Zero-length directions and collinear plane-defining points are outside the
corresponding incidence/direction constraint domain. Coincidence and distance
constraints do permit identical points, but a positive distance starting from
coincident points has no usable central derivative and may stall. Seed a
nondegenerate configuration instead.

## Custom constraints

Set kind to CUSTOM, a positive `residual_count`, a callback and optional `user`:

```c
static int curve(const double *x, size_t n, double *r, size_t m, void *user) {
    (void)n; (void)m; (void)user;
    r[0] = x[0]*x[0] + x[1]*x[1] - 25;
    return 1;
}
```

The callback receives all scalar values, including fixed variables. Fill every
output row with a finite number and return nonzero. Returning zero, writing
NaN/infinity or leaving a row unwritten rejects the evaluation. At the initial
guess or when neither finite-difference side can be evaluated this produces
EVALUATION_ERROR. An invalid trial step is instead rejected and damping is
increased. Derivative probing falls back to one side at domain boundaries.

Callbacks must be deterministic, must not retain the temporary arrays, and must
not mutate values, constraints, options or callback domain data during solving.
The solver uses no global state. Smooth residuals are preferred. Custom domain
checks can restrict radii, curve parameters or orientations, but there is no
built-in inequality/active-set solver. Callback execution time is the caller's
responsibility.

## Algorithm, options and diagnostics

`mulib_geosolve_default_options()` supplies:

| Option | Default | Meaning |
| --- | --- | --- |
| max_iterations | 100 | Total trial steps, including rejected steps |
| residual_tolerance | 1e-8 | Maximum absolute weighted residual |
| step_tolerance | 1e-12 | Maximum absolute step / max(1, absolute parameter) |
| gradient_tolerance | 1e-12 | Infinity norm of column-scaled J-transpose times residual |
| difference_step | 6e-6 | Probe size times max(1, absolute parameter) |
| initial_damping | 1e-3 | Initial dimensionless Levenberg damping |
| rank_tolerance | 1e-9 | Relative pivot threshold on column-scaled Jacobian |
| allocator | NULL | libc allocator |

Tolerances and damping must be finite and positive; difference_step is at most
0.1, rank_tolerance is less than 1, and damping is at most 1e15. A zero iteration
budget evaluates residuals and rank without taking a step. Already satisfied
systems return SOLVED even with a zero budget.

The solver forms central finite differences (one-sided where necessary), scales
Jacobian columns by their norms, and solves the augmented damped least-squares
system by column-pivoted Householder QR. It never forms normal equations.
Improving steps decrease damping; rejected steps increase it. At most 24 trials
are attempted per Jacobian, within the overall iteration budget. The final
undamped Jacobian is ranked at the returned point, including initially solved
systems. Rank is a numerical, local, first-order diagnostic, not a symbolic
count or proof of rigidity. `degrees_of_freedom = free_variables - rank`.
Residual rows beyond rank may be redundant **or inconsistent**.

`result` is optional. When supplied, it always receives a status, counts of
trial steps and full residual evaluations (including probes/rejections), row and
free-variable counts, rank, degrees of freedom, weighted cost, maximum absolute
weighted residual, and its constraint index. With no rows, worst_constraint is
`SIZE_MAX`. The first constraint wins exact ties. Cost is one half the sum of
squared weighted residuals. Fields other than status/counters are not reliable
after a negative status.

| Status | Publication / interpretation |
| --- | --- |
| SOLVED (0) | Publish best values; every weighted residual satisfies tolerance |
| STALLED (1) | Publish best values, but residuals remain unsatisfied |
| ITERATION_LIMIT (2) | Publish best values; budget exhausted before satisfaction |
| INVALID (-1) | Preserve original values; malformed descriptors, options or resource bounds |
| NO_MEMORY (-2) | Preserve original values; allocation failed |
| EVALUATION_ERROR (-3) | Preserve original values; initial residuals or derivatives cannot be evaluated |
| NUMERIC_ERROR (-4) | Preserve original values; linear algebra became unrepresentable |

Check the enum explicitly: this API does **not** return a boolean. STALLED does
not prove that a system is infeasible. Fixed coordinates are never committed
or perturbed. `mulib_geosolve_status_string()` returns static readable text.

### Bounds and numerical limitations

The dense solver supports at most 256 free variables, 4096 residual rows, 4096
constraint descriptors and 65536 total variables. Counts exceeding these bounds
are rejected before allocation. Limits are published as `MULIB_GEOSOLVE_MAX_*`
constants, not configurable implementation switches. Arithmetic for allocation
sizes is checked. For n total variables, k free variables and m rows, temporary
storage is `(2*n + 5*m + 3*k + m*k + (m+k)*k)` doubles plus `2*k` size_t indices.
QR costs O((m+k)*k^2) per trial; a Jacobian uses up to 2*k full residual
evaluations. This is intended for small/medium constraint systems, not sparse
million-variable models.

Normalize coordinate magnitudes and units before solving, especially when
mixing length, dimensionless angular and area equations; weights express their
relative scaling. Residual tolerance is not a bound on parameter error,
especially for small Jacobian coefficients or ill-conditioned systems. Large
coordinate offsets compared with feature sizes can make relative finite-difference
probes unsuitable. Very ill-conditioned data,
angles at wrap discontinuities, unsigned angles at 0/pi, abs at zero, coincident
distances and singular geometric arrangements need careful initial guesses.
The solver is double precision, without exact/adaptive predicates. It can reach
a local minimum or select a different branch. Use analytic constructions for
known intersections and seed more than one guess when branch coverage matters.

## Analytic constructions

Planar helpers use the existing double `mulib_geometry_point_t`.
`mulib_geosolve_line2_t` stores origin and a nonzero direction;
`mulib_geosolve_circle2_t` stores center and nonnegative radius.

- `line_line2`: infinite lines, classifying parallel distinct or coincident.
- `segment_segment2`: closed segments, including zero-length segments and
  collinear overlap. TWO returns the overlap's endpoints in the first segment's
  direction; ONE returns a crossing or endpoint contact.
- `line_circle2`: zero, one or two points ordered along the line direction.
  Projection geometry avoids the cancellation of a distant ray's quadratic
  discriminant.
- `circle_circle2`: zero, one or two points, or coincident positive circles.
  The first of two points is on the left of the directed center1->center2 line.
  Coincident zero-radius circles have one intersection. Scaled Heron factors
  avoid subtracting nearly equal squared distances.
- `circumcircle2`: circle through three finite noncollinear points, returning
  1/0 success rather than an intersection enum.

Spatial helpers reuse MuLIB's float linalg/cgeom descriptors, using double
intermediates and checked float outputs. A `mulib_geosolve_plane3_t` represents
`dot(normal,point) = offset`; normals need not be unit length.

- `plane_from_triangle3`: unit-normal plane oriented by the triangle's winding;
  1/0 success. Float rounding of the returned normal also affects the plane.
- `line_plane3`: takes `mulib_cgeom_ray_t`, but treats it as an **infinite line**,
  so negative parameters are allowed. Use collision's ray helpers for rays.
- `plane_plane3`: ONE returns the intersection **line** as `mulib_cgeom_ray_t`
  with unit direction and an origin closest to the coordinate origin.

Intersection enums are ERROR=-1, NONE=0, ONE=1, TWO=2, INFINITE=3. Analytic
output pointers are required; point-array outputs require capacity for two
points. Only ONE/TWO publishes output; NONE/INFINITE/ERROR preserves it.
Construction functions preserve output on failure. Input shapes are passed by
value. Nonfinite input, negative radii, zero directions/normals and
unrepresentable results fail explicitly.

These helpers do not apply an implicit epsilon: classifications use computed
zero/equality comparisons. Planar constructions use long-double intermediates
(which may be no wider than double on some platforms). Tangencies and nearly
parallel/collinear cases remain sensitive to floating-point rounding; this is
not exact arithmetic. Extreme inputs whose intermediates cannot be represented
are unsupported even if a mathematical output would be finite.

## Verification

`tests/test_geosolve.c` covers every built-in residual, anchored/mirrored planar
solves, overdetermined spatial trilateration, rank deficiency, redundant and
inconsistent equations, weighted fits, custom systems, one-sided domain probes,
empty and fixed systems, iteration/resource limits, invalid descriptors,
allocation-failure transactions, differently scaled and rank-deficient linear
systems, and analytic degeneracy/overlap/tangency. Deterministic generated cases
compare segment classifications with cgeom and verify circle intersections
against independent squared-distance classifications and distance checks.
It is discovered by CMake/CTest and `tools/check.py`.

```sh
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  tests/test_geosolve.c -lm -o /tmp/opencode/test-geosolve
/tmp/opencode/test-geosolve
```

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-geosolve.h`](../include/mulib-geosolve.h) |
| Implementation switch | `MULIB_IMPL_GEOSOLVE` |
| Repository-local includes | [mulib-alloc](mulib-alloc.md), [mulib-cgeom](mulib-cgeom.md), [mulib-geometry](mulib-geometry.md) |
| Complete example | [`examples/modules/geosolve.c`](../examples/modules/geosolve.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/geosolve.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
