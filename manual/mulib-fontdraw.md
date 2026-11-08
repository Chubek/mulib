# mulib-fontdraw

[Manuals](README.md) · [Library index](index.md)

Font loading, glyph outlines and rasterization using runtime-loaded FreeType 2.
Font formats, hinting, bitmap strikes, variable-font support and color renderers
are those enabled in the installed native library, rather than a partial
TrueType parser in MuLIB.

## Integration

Define `MULIB_IMPL_FONTDRAW` and `MULIB_IMPL_DYNLOAD` before including the header.
Link `-ldl` where needed. There is no compile/link requirement for FreeType
development files. Native names are `libfreetype.so.6`, `libfreetype.6.dylib`,
or `libfreetype-6.dll`; `open` accepts an explicit runtime path.
See `examples/modules/fontdraw.c`.

`open(font_path, face_index, pixel_height, runtime_path)` creates an independent
FreeType library and face, selects a positive pixel height and prefers a Unicode
charmap when available. Fonts without Unicode charmaps remain usable by glyph
id. Fixed-strike fonts require a size the native font supports; applications can
use `native` and `FT_Select_Size` for explicit strike selection. NULL indicates
invalid input/font, native errors, unavailable symbols or OOM. `size` changes
pixel height. Contexts are mutable and require caller synchronization.

## Rasterization and outline contracts

`glyph` maps a Unicode scalar; zero means missing/invalid.
`glyph` selects the Unicode charmap on every call, including after application
native-face use. If no Unicode charmap exists it returns zero rather than treating
Unicode values as legacy character codes.
`render` accepts
native `FT_LOAD_*` flags and render mode 0 normal, 1 light, 2 mono, 3 LCD,
4 vertical LCD or 5 SDF. It clears `FT_LOAD_RENDER` to honor the explicit render
mode, loads the glyph and copies the resulting bitmap. Native unsupported modes
or glyphs fail. Use `FT_LOAD_COLOR` (1<<20) for supported embedded color bitmaps;
native SVG/COLR rendering capabilities may require application hooks.

The owned bitmap is top-down with positive byte stride, native sample width and
height, bitmap-left/top bearings relative to the pen baseline, and advances
(normally 26.6 pixels). Pixel modes mirror FreeType: mono=1, gray=2, gray2=3,
gray4=4, LCD=5, LCD_V=6, premultiplied BGRA=7. LCD width/height are sample counts,
not necessarily physical pixel counts. Color bytes remain premultiplied BGRA.
No compositing or implicit color-space conversion occurs.

`alpha` produces a tightly packed owned alpha8 array for mono/gray/gray2/gray4
and BGRA. Gray levels are scaled to 0..255; mono/packed gray are MSB-first.
LCD modes are rejected because they require a subpixel-aware compositor.
Empty supported bitmaps succeed with NULL alpha. Feed alpha into
`mulib_distfield_generate` after adding transparent padding for glyph halos.

`outline` forces `FT_LOAD_NO_BITMAP`, clears `FT_LOAD_RENDER`, and copies the
scalable outline's points, tags, inclusive contour endpoints and flags. Low tag
bits distinguish on-curve, quadratic-control and cubic-control points. Units
are normally 26.6 pixels; `FT_LOAD_NO_SCALE` selects font units. Native contour
orientation/implied points are preserved. Bitmap-only glyphs fail.

Outputs must be zero-initialized. Return codes are 0/-1; failure does not modify
the output, though native face glyph state may have changed. Use
`bitmap_destroy`/`outline_destroy` for owned results, `free` for alpha. Copies
outlive their font. Checked allocation arithmetic and mode validation guard
wrapper-owned storage. Public bitmap structs passed to `alpha` must describe
valid accessible storage; the API cannot determine a pointer's allocation size.
Nonempty native bitmaps must have a recognized sample mode and sufficient pitch;
NULL storage and malformed outline point/tag/contour metadata are rejected before
copying. Negative-pitch native bitmaps are normalized to top-down copies, including
row padding. Outline vector records are copied as ABI bytes rather than aliased
through redeclared structure types. These checks do not sandbox native font parsing.

## Native extension and ABI

`native` borrows the `FT_Face`, enabling the complete FreeType API with matching
application development headers: transformations, variation coordinates, charmaps,
kerning, palettes, stroking, font tables and other native features. Do not destroy
that face; its lifetime ends at wrapper close.

The implementation independently declares the documented public FreeType 2
face/glyph-slot prefixes and bitmap/outline layouts, and copies prefixes with
`memcpy`. It does not access private fields. The ABI requires native C `long`,
pointer and enum representations compatible with FreeType's ordinary build.
`tools/check_text.py` optionally checks offsets/sizes and direct raster/outline
results against installed FreeType headers. The wrapper is not an independent
FreeType replacement or an exposure of every API through MuLIB names.
Reference: <https://freetype.org/freetype2/docs/reference/>. No native
implementation code is copied or distributed.
# Glyph metrics and paired shaping

`mulib_fontdraw_metrics(font,glyph,load_flags,&metrics)` loads a glyph without
rasterizing and returns width/height, horizontal bearings and x/y advances.
Default units are 26.6 pixels with y up; FT_LOAD_NO_SCALE selects font units.
FT_LOAD_RENDER is stripped. Returns 0/-1, output unchanged on failure; like
render/outline, this mutates the native face's glyph slot and requires caller
synchronization. Use shaping advances for laid-out text rather than replacing
them with these per-glyph metrics.

`mulib-glyph` opens paired HarfBuzz/FreeType contexts from one path/index/size,
exposes borrowed handles and renders the IDs produced by shaping. Owned copied
bitmaps/outlines remain valid after the paired font is closed.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-fontdraw.h`](../include/mulib-fontdraw.h) |
| Implementation switch | `MULIB_IMPL_FONTDRAW` |
| Repository-local includes | [mulib-dynload](mulib-dynload.md) |
| Complete example | [`examples/modules/fontdraw.c`](../examples/modules/fontdraw.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/fontdraw.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
