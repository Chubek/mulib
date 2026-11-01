# mulib-glx

MuLIB GLX is a software graphics library over memory and `mulib-drm` dumb buffers.
Define `MULIB_IMPL_GLX`, `MULIB_IMPL_DRM`, `MULIB_IMPL_LINALG`; link `-lm`.
This API has its own namespace and is independent of X11 GLX/OpenGL contexts.

## Surfaces and DRM

`create(width,height)` owns a transparent pixel allocation and a depth buffer.
Dimensions are 1..16384. `wrap` borrows caller pixels while owning its depth
buffer. Pixels must have uint32 alignment. Pitch is **bytes**, divisible by four
and at least width*4; byte capacity must cover height*pitch. Invalid sizes,
overflow and allocation failure return NULL. Depth costs width*height*sizeof(float).
Contexts are mutable and require external synchronization. Do not copy them.

`map_drm` maps a caller-owned 32-bpp dumb buffer through `mulib_drm_dumb_map`,
then borrows its data using the driver's pitch. The buffer/mapping must outlive
the GLX context. The caller still owns a successful mapping if GLX allocation
fails. Destroy GLX before destroying the dumb buffer:

```c
mulib_drm_device_t device = MULIB_DRM_DEVICE_INIT;
mulib_drm_dumb_t buffer = MULIB_DRM_DUMB_INIT;
/* Check each result; on failure release acquired resources. */
if (mulib_drm_open(&device, "/dev/dri/card0") == MULIB_DRM_OK) {
    if (mulib_drm_dumb_create(&device, 640, 480, 32, &buffer) == MULIB_DRM_OK) {
        mulib_glx_t *g = mulib_glx_map_drm(&buffer);
        if (g) { mulib_glx_clear(g, 0xff203040); mulib_glx_destroy(g); }
        /* Check failure and retry retained resources as documented by DRM. */
        int cleanup = mulib_drm_dumb_destroy(&buffer);
        (void)cleanup;
    }
    (void)mulib_drm_close(&device);
}
```

Modesetting, framebuffer registration/page flips and GPU command submission are
outside the existing DRM API; mapping/drawing does not itself display an image.
Offscreen surfaces work without a DRM device, including on non-Linux systems.

## Drawing

Pixels are native uint32 values `0xAARRGGBB`, straight alpha. `pixel`, rectangles,
lines, triangles and blits blend source-over, including destination alpha.
Coordinates start at top-left, Y increases downward. Integer rectangle bounds
are half-open. Scissor is clamped to the surface; empty scissor draws nothing.
`clear` ignores scissor, replaces pixels and resets depth to infinity, preserving
row padding. Accessors borrow pixels and report byte pitch.

Lines are clipped before rasterization. Flat triangles and shaded triangles use
pixel-center samples and a top-left coverage rule, so shared triangle edges do
not blend twice. Degenerate triangles succeed without drawing. Shaded vertices
contain screen X/Y, finite depth and RGBA; interpolation is screen-linear.
The depth test is strictly less. A zero-alpha fragment does not update depth.
An optional C callback returns an ARGB fragment; it must not recursively draw or
mutate/destroy the context. NULL callback packs interpolated RGBA. Perspective
division, homogeneous clipping and perspective-correct varyings are caller work.

`blit` validates its source layout before drawing, clips destination coordinates,
and blends. Source must not alias destination. `rgba` clamps components and rounds;
NaN maps to zero. Primitives with an int result return 1 success, 0 invalid input.
NULL-safe void operations simply do nothing.

`examples/modules/glx.c` draws an interpolated triangle. Tests exercise pitch
padding, alpha, scissor, shared edges, depth, lines and blits in memory.
