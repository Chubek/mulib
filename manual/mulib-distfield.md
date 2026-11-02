# mulib-distfield

Signed Euclidean distance fields for scalable glyph masks and other binary
silhouettes. This module is independent of native font and Unicode libraries.

## Integration

Define `MULIB_IMPL_DISTFIELD` in one C11 translation unit before including the
header. Unix builds link `-lm`. Example: `examples/modules/distfield.c`.

`generate(mask, width, height, stride, spread, &field)` borrows an alpha8 input
with byte stride and returns a malloc-owned, tightly packed `width*height`
field. Input alpha >=128 is inside. The input is never modified. Return 0 on
success and -1 for invalid arguments/dimensions, nonpositive/nonfinite spread
or allocation failure; the output pointer remains unchanged on failure.
Empty dimensions succeed with NULL output (stride must still be >=width).

Dimensions are limited to `INT_MAX`, all allocation products and source-span
arithmetic are checked. Nonempty input needs valid readable storage covering
`(height-1)*stride+width` bytes. Free the result with `free`.

## Numerical convention

Two separable squared-Euclidean transforms find the nearest opposite-class
pixel centers. Subtract half a pixel from center distance, positive inside and
negative outside. Pixels outside the image count as background, so fully filled
masks still have a well-defined boundary; fully empty masks saturate outside.
Generation uses O(width*height) time and storage plus O(max(width,height))
scratch space, rather than scanning all candidate pixels for every output.

The encoded byte is

```
round(255 * clamp(0.5 + signed_distance / (2*spread), 0, 1))
```

`spread` is in input pixels. Add at least `ceil(spread)` transparent padding
around glyph bitmaps to retain an exterior halo. `distance(sample, spread)`
decodes the quantized distance. A bilinearly sampled normalized texture value
can be passed to `coverage(value, edge_width)`, which returns smoothstep
coverage around 0.5. The edge width should be adapted to screen-space derivatives
by the renderer; invalid inputs produce zero coverage.

## Scope and validation

This is a single-channel, thresholded-mask SDF, not a multi-channel edge-colored
MSDF, vector-outline exact distance solver or GPU atlas manager. Original
antialiasing below/above the threshold is intentionally reduced to membership.
Texture coordinates, padding placement, GPU upload and atlas packing belong to
the renderer. Very large spread values lose quantized edge precision.

`tests/test_distfield.c` compares every output pixel against an independent
brute-force Euclidean reference for deterministic random masks, empty and solid
masks, plus malformed dimensions and numerical edge cases. Allocation-failure
coverage is in `tests/test_text_allocations.c`.
