# mulib-fontshape

Real OpenType shaping through HarfBuzz, rather than a cmap/kerning approximation.
Features, contextual substitutions/positioning, script shaping and variable-font
coordinates use the native HarfBuzz engine and the selected font's tables.

## Integration

Define `MULIB_IMPL_FONTSHAPE`, `MULIB_IMPL_DYNLOAD`, and `MULIB_IMPL_UTF8` before
including `mulib-fontshape.h`. Link `-ldl` where needed. No HarfBuzz headers or
link-time dependency are required. The runtime needs
`hb_blob_create_from_file_or_fail` (HarfBuzz 2.6+), OpenType font functions and
the other documented C APIs used by the wrapper. Native names are
`libharfbuzz.so.0`, `libharfbuzz.0.dylib`, and `libharfbuzz-0.dll`; an explicit
runtime path overrides discovery. See `examples/modules/fontshape.c`.

## Font and segment setup

`open(path, index, scale, library_path)` owns its file-backed blob/font/runtime.
`index` selects a collection face (native named-instance bits are preserved).
Both scale axes use the positive integer `scale`; `pixel_size*64` is convenient
for 26.6-pixel output and matches fontdraw's default metrics. An invalid or
unreadable font, unavailable backend, nonpositive scale or OOM returns NULL.
The collection face number (low 16 index bits) must exist; invalid indices are
rejected instead of silently selecting face zero. Named-instance bits retain
HarfBuzz's native semantics and are not independently validated by this wrapper.
`close` releases the font and runtime. Calls sharing a font must be serialized.

`shape` consumes one uniformly directed/script UTF-8 run. Optional direction is
`ltr`, `rtl`, `ttb` or `btt`; script is a four-byte ISO 15924 tag; language is a
BCP-47-style language string. Omitted direction/script are guessed by HarfBuzz;
omitted language is `und`, independent of process locale. The caller performs
paragraph bidi resolution, script segmentation and font fallback. `mulib-bidi`
provides visual directional runs but is not itself a script itemizer.

Feature strings use HarfBuzz syntax (`liga=0`, `kern`, `aalt=2`, ranged features).
Ranges refer to input UTF-8 cluster offsets. All feature strings are parsed
before shaping; invalid syntax fails. `variations` similarly parses axis strings
such as `wght=700`; it replaces previous coordinates and zero count resets them.
Unknown axes follow HarfBuzz's native behavior. No synthetic bold/italic is used.
Axis values must be finite. Explicit script tags must contain four ASCII letters;
unknown alphabetic tags retain HarfBuzz's native script interpretation.

## Output and errors

Zero-init `mulib_fontshape_run_t` before passing it. Success returns an owned
array containing glyph ids, byte clusters, x/y advances and offsets in font-scale
units (positive y up). RTL output is in native visual glyph order and clusters
can descend; ligatures can merge multiple characters. Glyph zero is the font's
missing-glyph indication, not a wrapper error. Destroy with `run_destroy`.
Results remain valid after font close.

Strict UTF-8 is validated before invoking the backend, including overlong,
surrogate and out-of-range rejection. Embedded NULs are supported; NULL input
requires zero length. Empty input succeeds with no glyphs. Byte lengths are
limited to `INT_MAX`; counts and allocation products are checked. Return 0/-1,
with output unchanged on failure. Inputs/options are borrowed during the call.

`covers` tests nominal glyph coverage for one Unicode scalar (1/0, -1 invalid),
without claiming that a nominal glyph proves support for an entire shaped
sequence. `mulib-pglayout` adds script/grapheme itemization and ordered font-set
fallback on top of this single-run primitive.

`native` borrows the `hb_font_t` for advanced HarfBuzz APIs, custom functions,
metrics and other native functionality; use matching development headers in
application code and never destroy the borrowed object. The wrapper does not
re-expose every HarfBuzz API, provide font fallback, or claim a new independent
HarfBuzz implementation.

## Verification

`tests/test_font_stack.c` covers ligature toggles, Arabic RTL clusters, malformed
UTF-8, bad options and font loading. `tools/check_text.py` optionally compares
glyph ids, clusters and positions with direct HarfBuzz calls.
ABI reference: <https://harfbuzz.github.io/>. Only independently expressed public
ABI declarations are included; no third-party implementation is vendored.
Native glyph records are copied as ABI bytes, avoiding effective-type aliasing
through locally redeclared structure pointers. See [text hardening](text-hardening.md).
# Glyph metrics and ligature integration

`mulib_fontshape_scale(font)` returns the scale configured at open (zero for
NULL). `mulib_fontshape_metrics(font,&metrics)` queries horizontal ascender,
descender and line gap; descender is normally negative. `mulib_fontshape_extents`
queries a glyph's x/y bearing, width and height. These are integer font-scale
units, y up, with 0/-1 transactional results. `mulib-layout` uses real font
metrics for baselines rather than assuming fixed proportions of font size.

`mulib_fontshape_carets(font,glyph,direction,&positions,&count)` queries GDEF
ligature carets in ltr/rtl/ttb/btt direction. Positions are malloc-owned, signed
font-scale units; absence of caret data succeeds with count zero. Free the array
with `free`. Outputs are unchanged on failure. `mulib-ligature` adds explicit
optional-feature controls and source cluster mapping; `mulib-glyph` pairs shaped
IDs with the same font face for FreeType rendering.
