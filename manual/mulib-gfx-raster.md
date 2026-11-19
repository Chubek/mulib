# Shared CPU surfaces and ordered raster commands

These additive C11 headers provide a headless software baseline, not a GPU or
display protocol implementation. Existing `mgx_*` types and struct layouts are
retained; `mulib_glx_{point,rect,affine,surface,drawcmd}_t` aliases are available.

## Storage and lifetime

Pixels are native-endian `uint32_t`, straight `0xAARRGGBB`. Rectangles are
half-open, origin top-left. `mgx_surface.stride` and `mgx_wrap_capacity` capacities
are **words**. `mulib_glx_surface_borrow[_capacity]` pitch and capacity are
**bytes**. The minimum accessible span is `(height - 1) * stride + width` words;
padding after the final row is not required. Overflow, short stride, short
declared capacity, null/misaligned pointers and invalid dimensions are rejected.

Use `mulib_glx_surface_borrow_capacity` for externally sized allocations.
`mgx_wrap` and the original borrow API cannot discover an allocation's capacity:
the caller must guarantee its accessible span. `mgx_surface_valid` checks
layout/address arithmetic, **not** pointer provenance, actual allocation length,
or whether the allocation has been freed. Struct layout is unchanged, so no
capacity is retained or rechecked against allocation size after public fields
are modified. Do not mutate dimensions/stride to exceed the validated allocation.

Initialize into empty/zero objects; release before reuse. Do not copy owning
structs. A view borrows the original storage and has independent local
coordinates and a full local clip; source drawing clip does not constrain view
creation. Releasing a view does not free its owner. The owner must stay alive
until all views and deferred consumers have finished. In-place view creation
(`out == source`) is rejected. `mulib_glx_surface_snapshot` creates an independent
owned copy, including the source clip but excluding row padding.

All mutable objects are render-thread-owned. No automatic reference counting,
cross-thread pinning, or asynchronous presentation fence is provided here.

## Drawing and clipping

Core primitives and blits intersect drawing clips with the physical surface.
Negative/overflowing rectangle endpoints are calculated with widened arithmetic.
NaN/Inf primitive parameters are ignored; finite oversized primitive bounds are
saturated before clipping. `mgx_clear` deliberately ignores the clip and performs
SOURCE over active pixels; row padding is untouched.

Clip stacks are bounded to 64 scopes. Failure leaves the stack/clip unchanged.
Use one live surface per nonempty stack, and balance scopes before resizing or
releasing that surface. Pop restores the exact saved clip.

Source-over uses straight-alpha input/output with unrounded coverage during
color calculation. Multiply/screen/saturated-component-add include both
nonoverlapping source and backdrop contributions. These operations are byte
space arithmetic, **not** linear-light color management. ADD is not Porter-Duff
PLUS.

`mulib_glx_texture_draw_checked` and `mulib_glx_composite_checked` return 1 on
success (including empty clipped draws), 0 for invalid arguments/allocation
failure. Before writing, any potentially overlapping storage, including distinct
view bases/strides, is copied. Failure leaves the target unchanged. Original void
wrappers remain and discard the status; new callers should use checked variants.
Full-source snapshots can cost memory; overlap detection includes row padding
and can conservatively copy buffers whose active pixels do not overlap.

Sampling ignores source drawing clip. Normalized sample endpoints select first
and last texel centers. Drawing scales edge-to-edge with pixel-center alignment;
an unscaled draw reproduces exact texels. Bilinear interpolation uses
premultiplied components internally and returns straight ARGB to avoid fringes
from transparent RGB.

## Formats

`mulib_glx_pixelformat_convert` accepts source/destination byte capacities,
byte pitches, formats, width and height. It supports native-endian ARGB8888,
XRGB8888, ABGR8888 and RGB565, including unaligned buffers and overlapping
in-place conversions. It validates all spans before writing, snapshots overlap,
and leaves padding unchanged. Invalid formats are rejected. XRGB forces opaque
alpha; RGB565 drops alpha without compositing onto a background. These are
integer formats, not an assertion about a display server's byte order or visual.

## Deferred resources and painter order

`mulib_glx_drawlist_add` copies the command, but **borrows both the image struct
and its pixels**. Both must outlive rendering. A render queue also borrows the
list and its command array; do not reset, mutate or release a queued list.
Neither list reset/release nor queue free releases borrowed resources.

For independent deferred storage, capture a `mulib_glx_drawlist_snapshot_t`:

```c
mulib_glx_drawlist_snapshot_t frozen = {0};
/* list and its source images must be valid during capture. */
if (mulib_glx_drawlist_snapshot(&frozen, &list)) {
    /* Original list/images may now be released. */
    if (mulib_glx_renderqueue_submit(&queue, &frozen.draws)) {
        int ok = mulib_glx_renderqueue_execute_checked(&queue, &target);
        (void)ok; /* Application should handle failure. */
    }
    mulib_glx_drawlist_snapshot_release(&frozen);
}
```

A frozen snapshot owns independent commands and copies of every image.
Do not mutate/copy it, and release it only after its queued consumers finish.
It captures image contents at capture time, not at later execution time.

Rendering preserves command insertion order and queue submission order,
including overlapping translucent commands. No texture/color sorting occurs.
Checked rendering prevalidates every command and restores the original target
clip on every exit. Later allocation failure can leave a painted prefix; it is
not a whole-frame transaction. Queue validation failure leaves it untouched.
Once painting starts, execution consumes the queue even on allocation failure,
so retry does not accidentally replay already composited alpha commands.

## Regions

Regions are rectangle collections, not canonical nonoverlapping unions.
Overlaps/duplicates are retained. Nonempty rectangles must contain representable
pixel coordinates (exclusive endpoints may equal `INT_MAX + 1`); insertion
rejects larger endpoints. Subtraction uses widened endpoints and is
transactional on allocation/representation failure.

## Regression

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude tests/test-gfx-raster.c -lm -o /tmp/opencode/test-gfx-raster
/tmp/opencode/test-gfx-raster
cc -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -Iinclude tests/test-gfx-raster.c -lm -o /tmp/opencode/test-gfx-raster-san
ASAN_OPTIONS=detect_leaks=1 /tmp/opencode/test-gfx-raster-san
```

Tests include all alpha pairs against an independent blend reference,
deterministic randomized overlapping-view blits, guard/padding checks, extreme
coordinates and rectangle/primitive properties, short capacities,
unaligned/in-place pixel conversion, painter
ordering, frozen resources after original image release, and injected allocation
failures (including partially constructed snapshots). No hardware access
is needed. This is not a complete fuzzing, 32-bit, color-management, or production
renderer certification.
