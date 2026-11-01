# mulib-collision

Discrete primitive collisions and ray queries using cgeom's shared geometry.
Define `MULIB_IMPL_COLLISION`, `MULIB_IMPL_CGEOM`, `MULIB_IMPL_SIMDMATH`,
`MULIB_IMPL_LINALG`; link `-lm`.

All queries return **-1 invalid, 0 miss, 1 hit**. Touching counts as a hit.
Output pointers are optional and remain unchanged on invalid input or miss.
Nonfinite coordinates/radii, reversed boxes and negative radii are invalid.
Boxes may be flat and overlap spheres may have radius zero.

## Contacts

Sphere/sphere, AABB/AABB and sphere/AABB return a unit normal and penetration
depth. Moving the **first** object by `-normal * depth` separates the objects to
contact. The second object's motion is not applied automatically. Containment
uses the distance needed to leave the containing object, not merely the width
of the overlap region. Coincident spheres choose +X; box ties choose the first
minimum axis. Sphere/box handles centers inside/on the box by selecting its
nearest face. Depth beyond float range returns -1 without output.

`convex2` tests separating axes from both input polygons. Inputs must already be
convex, ordered boundary loops, with 3..4096 vertices and nonzero area. Either
winding is accepted, touching is included. The function checks size, finite
coordinates and nonzero area; convexity/order are caller preconditions. Use
`cgeom_convex_hull` to construct such inputs. It performs no allocation.

## Rays

Rays are `origin + t*direction`; directions need not be normalized. Bounds must
be finite, `0<=t_min<=t_max`, and are inclusive. Use `FLT_MAX` for a large bound,
not infinity. A zero/nonfinite direction is invalid.

Sphere queries use a cross-product discriminant and cancellation-reduced roots
to avoid false hits from large longitudinal ray distances. Box queries use
slabs, explicitly handling zero direction components. Both choose the first
surface inside the interval; a ray starting inside normally finds the exit
surface. Triangle queries are two-sided; parallel or degenerate triangles miss.
Triangle normals follow vertex winding, sphere/box normals point outward.
Hits report ray parameter, point, unit normal and triangle barycentric weights;
nontriangle barycentric fields are zero. Zero-radius sphere normals use +X.

Floating-point algorithms do not supply exact predicates, continuous rigid-body
simulation, contact manifolds or response integration. `examples/modules/collision.c`
demonstrates a hit; `tests/test_graphics_math.c` covers tangency, containment,
inside rays, interval clipping, non-unit directions and triangle weights.
