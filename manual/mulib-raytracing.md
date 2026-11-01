# mulib-raytracing

A deterministic CPU ray renderer using the collision library for primitive hits
and GLX for its destination. Define `MULIB_IMPL_RAYTRACING`,
`MULIB_IMPL_COLLISION`, `MULIB_IMPL_CGEOM`, `MULIB_IMPL_SIMDMATH`,
`MULIB_IMPL_LINALG`, `MULIB_IMPL_GLX`, `MULIB_IMPL_DRM`; link `-lm`.

## Scene and ownership

Scenes borrow immutable object/light arrays. Each object embeds a sphere or
triangle and a material (linear RGB diffuse/emission and scalar reflectivity).
Point lights supply position, linear color and inverse-square intensity. Scene
ambient and background are linear RGB. No allocations or resource ownership are
hidden in scene descriptors. Arrays must remain valid throughout queries/rendering.

`scene_valid` enforces at most 65536 objects and 64 lights, finite coordinates
within +/-1e6, sphere radius in `(0,1e6]`, diffuse/light color/reflectivity in
`[0,1]`, emission/background/ambient in `[0,1000]`, and intensity in `[0,1e6]`.
Empty arrays may be NULL. Degenerate triangles are allowed but do not intersect.

## Queries and shading

`intersect` returns -1 invalid, 0 miss, 1 hit, preserving optional output on
miss/error. It preserves the supplied direction's parameterization and finds
the nearest surface in inclusive finite `0<=t_min<=t_max`. Ties keep the first
object. Output includes object index, collision hit and front-face flag.

`sample` normalizes its ray and evaluates linear RGB, returning 1/0 and preserving
output on invalid input. Origin must satisfy the scene coordinate bound. It
supports 0..8 recursive mirror bounces. Shading combines diffuse ambient,
Lambert cosine times inverse-square point illumination, hard shadow rays and
emission. Reflection mixes direct diffuse with reflected radiance according to
reflectivity. At the recursion limit, the direct diffuse/emission result is used.
Geometric normals are face-forwarded for two-sided shading. Secondary origins
use a position-scaled float offset to reduce self-intersection.

`render` validates scene/camera before writing pixels, then traces one ray through
each pixel center. Camera uses eye, target, up and vertical FOV in radians
`(0,pi)`. Coincident eye/target or parallel up fails. Output clamps linear RGB to
`[0,1]`, converts to sRGB and writes opaque GLX pixels through its scissor. It
does not change GLX's triangle depth buffer. Success is 1, invalid input 0.

The algorithm is linear scene traversal per ray, with no acceleration structure,
stochastic global illumination, transmission, textures, antialiasing or worker
threads. Numerical hit predicates follow the collision module's documented
float limitations. Geometry and material limits keep the shading computation
bounded but do not imply real-time performance for maximum-size scenes.

`examples/modules/raytracing.c` writes a 96×64 `raytracing.ppm` without a display.
`tests/test_graphics_render.c` checks nearest hits, background, emissive pixels,
reflection and a point-light occluder.
