# mulib-raytracing

[Manuals](README.md) · [Library index](index.md)

A CPU ray renderer using the collision library for primitive hits. It provides
both the original center-sampled direct renderer and a seeded path tracer, with
linear RGB or GLX image output for the latter. Define `MULIB_IMPL_RAYTRACING`,
`MULIB_IMPL_COLLISION`, `MULIB_IMPL_CGEOM`, `MULIB_IMPL_SIMDMATH`,
`MULIB_IMPL_LINALG`, `MULIB_IMPL_GLX`, `MULIB_IMPL_DRM` in one C translation
unit; link `-lm`. The new entry points are `static inline`; consumers include
the header without defining implementation switches.

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

## Queries and legacy direct shading

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

## Seeded path tracing and antialiasing

Use `mulib_raytracing_path_options_t` with any 64-bit seed, 1–1024 samples per
pixel, and a maximum of 0–16 secondary bounces. Zero bounces still evaluate
visible emission and direct point lighting at the first hit, or environment
radiance on a miss. Use more samples for less noise and more bounces for longer
light paths; these settings control different sources of error.

- `mulib_raytracing_camera_ray(camera, w, h, x, y, u, v, &ray)` generates a
  unit direction through pixel `(x,y)` at subpixel offset `(u,v)`, each in
  `[0,1)`. Pixels start at the top left. The pinhole camera uses vertical FOV
  in radians `(0,pi)` and aspect ratio `w/h`. Eye and target obey the scene's
  coordinate bound. Coincident eye/target, nonfinite or zero up, and up within
  `1e-6` in cross-product magnitude of parallel/antiparallel (after
  normalization) are invalid. Width and height must be 1–16384.
- `mulib_raytracing_path_sample(scene, ray, options, &rgb)` averages paths
  with a fixed primary ray (no subpixel jitter). The ray origin must obey the
  scene coordinate bound and the direction is normalized internally.
- `mulib_raytracing_path_pixel(scene, camera, w, h, x, y, options, &rgb)` averages
  paths with independent uniform subpixel jitter (a box filter). Its result
  matches the corresponding full-frame linear RGB result for the same inputs.
- `mulib_raytracing_path_render(scene, camera, options, image)` writes unclamped
  linear RGB to caller-supplied `mulib_linalg_vec3_t` rows. The image descriptor
  counts `stride` and `capacity` in pixels, not bytes. Stride must be at least
  `width`; the last written pixel must be within `capacity`. Padding is not
  touched. The caller supplies valid aligned storage of the stated capacity.
- `mulib_raytracing_path_render_glx(scene, camera, options, surface)` averages
  in linear RGB before clamping and conversion to opaque sRGB. It overwrites
  through the existing scissor, leaves depth alone, and still traces pixels
  outside the scissor. For HDR or custom tone mapping, use the linear output.

The path and camera calls return 1 on success or 0 on invalid input without
changing the output. Inputs must remain live and immutable during a call; image
or surface storage must not alias inputs. No storage is retained or allocated by
the ray tracer. Callers may parallelize independent pixel queries with immutable
inputs and disjoint outputs; no shared RNG or internal workers are used.

```c
/* scene, camera and a 320 * 200 array of mulib_linalg_vec3_t are caller-owned. */
mulib_raytracing_path_options_t options = { .seed = 42,
                                          .samples_per_pixel = 32,
                                          .max_bounces = 4 };
mulib_raytracing_image_t image = { .pixels = pixels, .capacity = 320 * 200,
                                  .stride = 320, .width = 320, .height = 200 };
int ok = mulib_raytracing_path_render (&scene, camera, options, image);
```

At each surface the integrator adds two-sided emission and explicitly
evaluates visible inverse-square point lights with a Lambertian `cos(theta)/pi`
factor. Diffuse weight is `(1 - reflectivity) * diffuse`; the other lobe is a
perfect, untinted mirror with weight `reflectivity`. One lobe is sampled per
bounce, with cosine-weighted directions for diffuse transport. Rays that escape
collect the constant `background` as environment radiance. Emissive geometry
lights other surfaces when paths hit it; it is not explicitly sampled as an
area light, so small emitters can be noisy. The path tracer validates but does
not use the legacy `ambient` field. It does not model glossy lobes, transmission,
participating media, textures, or depth of field.

An integer SplitMix64-based stream is derived from seed, pixel coordinate and
sample index. Within a build and floating-point environment, results are
repeatable and independent of traversal order. Bit-for-bit results across
platforms, compilers, floating-point modes, or libm implementations are not
promised. Finite-input checks assume ordinary floating-point semantics, not
`-ffast-math`.

Numerical hit predicates inherit the collision module's limitations. Paths
ignore hits closer than `1e-4` world units in ray distance. Secondary and shadow
origins shift along the face-forwarded geometric normal by
`max(1e-4, 8 * FLT_EPSILON * max(abs(point)))`; point lights within twice that
distance from the hit are skipped to avoid the inverse-square singularity and
ambiguous visibility. Use consistent scene scale: small or very thin features
and nearby lights can be missed, especially far from the origin.

Each path visits at most `max_bounces + 1` surfaces and casts at most one shadow
ray per point light per surface. Each ray scans the scene, with early exit for
shadow occlusion. Worst-case work scales as pixels × samples ×
`(max_bounces + 1)` × `(lights + 1)` × `(objects + 1)`; there is no acceleration
structure or frame-time budget. Use modest counts for interactive workloads.

`examples/modules/raytracing.c` writes a legacy-rendered 96×64 `raytracing.ppm`
without a display. `sh tests/run-raytracing.sh` exercises both renderers,
including seed repeatability, jittered edge coverage, diffuse and mirror
transport, shadows, and output bounds. Set `CC=gcc` or `CC=clang`, and optionally
`SANITIZE=1` for AddressSanitizer and UndefinedBehaviorSanitizer.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-raytracing.h`](../kernels/mulib-raytracing.h) |
| Implementation switch | `MULIB_IMPL_RAYTRACING` |
| Repository-local includes | [mulib-collision](mulib-collision.md), [mulib-glx](mulib-glx.md) |
| Complete example | [`examples/modules/raytracing.c`](../examples/modules/raytracing.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/raytracing.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
