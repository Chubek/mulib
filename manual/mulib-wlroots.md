# mulib-wlroots

[Manuals](README.md) · [Library index](index.md)

Wlroots-style **logical output geometry and bookkeeping**, not a binary binding
to wlroots, a compositor, backend, scene graph or seat implementation. This
deliberately avoids wlroots' version-specific struct layout and external headers.

## Integration and boxes

Define `MULIB_IMPL_VEC`, `MULIB_IMPL_WLROOTS` before including the header in one
C11 translation unit. Pure C11, no wlroots/Wayland runtime required.
`mulib_wlroots_box_t` is a copyable int32 box; sizes must be nonnegative and
right/bottom endpoints fit int32. Points use half-open intervals. `box_valid`
accepts empty boxes; `box_contains` never matches them. `box_intersection`
returns 1 with a nonempty intersection, 0 otherwise (output unchanged).

`box_transform(&box,transform,width,height,&out)` uses wl_output_transform
values 0..7: normal, 90, 180, 270, flipped, flipped-90, flipped-180, flipped-270.
The input box must be within the nonnegative canvas; transformed boxes use the
transformed canvas (width/height swapped for odd transforms). Example 90-degree
formula: `(height-y-box.height, x, box.height, box.width)`. Output may alias input.
Invalid input leaves output unchanged; arithmetic uses widened intermediate sums.
No scale conversion, floating-point coordinates or native output access.

## Layout ownership

Initialize `mulib_wlroots_layout_t` with `layout_init`, release with repeatable
`layout_free`. It owns a MuLIB vector; never shallow-copy or mutate its fields.
`layout_set(&layout,id,&box)` copies a nonempty valid box and nonzero caller id,
updates an existing id in place or appends. Returns 1, or 0 on invalid/OOM without
changing prior entries. `layout_remove` returns 1 if removed, 0 if absent.
`layout_at` returns the first-added overlapping output id or zero. Overlaps are
allowed; update preserves order and removal preserves remaining order.
No pointers to native outputs are retained; caller handles hotplug id lifetimes.

`cc -std=c11 -Iinclude examples/modules/wlroots.c -o wlroots`.
Tests exercise all eight transforms, invalid coordinates, boundaries, overlap,
update, removal and repeated cleanup without starting a desktop session.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-wlroots.h`](../include/mulib-wlroots.h) |
| Implementation switch | `MULIB_IMPL_WLROOTS` |
| Repository-local includes | [mulib-vec](mulib-vec.md) |
| Complete example | [`examples/modules/wlroots.c`](../examples/modules/wlroots.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/wlroots.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
