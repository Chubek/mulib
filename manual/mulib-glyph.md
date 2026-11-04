# Positioned glyphs and paired fonts

`include/mulib-glyph.h` bridges `fontshape`, `fontdraw`, `sysfonts`, `ligature`,
and the existing `pglayout` Unicode itemizer. It has C11 implementation and C++
declarations, without compile-time third-party dependencies.

## Integration and ownership

Define `MULIB_IMPL_GLYPH`, `MULIB_IMPL_LIGATURE`, `MULIB_IMPL_FONTDRAW`,
`MULIB_IMPL_SYSFONTS`, `MULIB_IMPL_PGLAYOUT`, `MULIB_IMPL_FONTSHAPE`,
`MULIB_IMPL_BIDI`, `MULIB_IMPL_UNIBREAK`, `MULIB_IMPL_UNIPROC`,
`MULIB_IMPL_DYNLOAD`, `MULIB_IMPL_STYLESHEET`, `MULIB_IMPL_CSS`,
`MULIB_IMPL_XML`, `MULIB_IMPL_YAML`, `MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`,
`MULIB_IMPL_STRBUF`, and `MULIB_IMPL_UTF8` in exactly one TU before inclusion.
The complete runnable `examples/modules/glyph.c` shows all switches.
Linux build: `cc -std=c11 -Iinclude examples/modules/glyph.c -lm -ldl -pthread`.

`mulib_glyph_font_open(path,index,pixels)` owns paired HarfBuzz/FreeType contexts
for the same file and face. Positive integer pixels are limited to INT_MAX/64;
indices to INT_MAX. `font_match` borrows a Fontconfig catalog, resolves a pattern,
opens the pair and releases the temporary match record. Constructors return NULL
on invalid font/backend/allocation failure; `font_close(NULL)` is harmless.

`font_shape` and `font_draw` borrow contexts; do not destroy them independently.
Do not resize/change variations independently when using them as a pair. Callers
requiring variable-font synchronization can manage native font contexts directly.
`mulib_glyph_render` renders an ID with FreeType flags/mode, preserving fontdraw's
zero-initialized output and bitmap ownership conventions. It does not composite.

## Shaping

`mulib_glyph_shape(fontset, paragraph, bytes, start, length, &run)` uses strict
UTF-8 and paragraph context, ICU bidi/script/grapheme boundaries and ordered font
fallback. Set font scales in 26.6 pixels. It reuses `pglayout_shape_line` and its
coverage policy; see that manual for itemization limits. Requested ranges must
be grapheme-aligned and cannot cross a bidi paragraph separator. Fonts/ICU are
borrowed during the call. No fallback font is opened implicitly.

Output must be zeroed. On success (0), the run owns a visual-order glyph array,
logical advance and resolved paragraph level. On failure (-1), output is
unchanged. `run_destroy` frees and zeroes the result. Allocations are overflow
checked and input limits follow ICU/HarfBuzz's signed length limits.

Each glyph contains ID, font index, bidi level, absolute source byte start/length,
baseline origin x/y and advance. Ligature's monotone cluster mapper supplies
source intervals; several marks/glyphs can share one source interval. Positions
are doubles in pixels with y down, including shaping offsets. The initial
baseline is (0,0). A result owns no font handles and can outlive shaping, but
rendering requires the original faces. Fontshape supplies y-up scaled ink
extents; fontdraw supplies unrendered FreeType metrics and copied bitmap/outline
data. Neither logical advance nor bitmap width should be substituted for the
other when rendering combining marks or ligatures.

Tests: `tests/test_text_layout.c`, including real paired rendering and source
mapping, and existing `tests/test_font_stack.c`.
