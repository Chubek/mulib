# Unicode text layout and rectangles

[Manuals](README.md) · [Library index](index.md)

`include/mulib-layout.h` provides an owned, renderer-neutral text layout in C11
with C linkage for C++. It composes ICU bidi/Unicode boundaries, HarfBuzz
OpenType shaping, font fallback, and `mulib-glyph`. The original weighted
rectangle API remains available.

## Integration

Define these switches in exactly one implementation TU, before including
`mulib-layout.h` (applications control implementation placement):

```c
#define MULIB_IMPL_LAYOUT
#define MULIB_IMPL_GLYPH
#define MULIB_IMPL_LIGATURE
#define MULIB_IMPL_PGLAYOUT
#define MULIB_IMPL_FONTDRAW
#define MULIB_IMPL_FONTSHAPE
#define MULIB_IMPL_SYSFONTS
#define MULIB_IMPL_BIDI
#define MULIB_IMPL_UNIBREAK
#define MULIB_IMPL_UNIPROC
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_STYLESHEET
#define MULIB_IMPL_CSS
#define MULIB_IMPL_XML
#define MULIB_IMPL_YAML
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-layout.h"
```

The page-layout dependency supplies the existing paragraph-aware itemizer; its
document dependencies therefore also need implementations. Native ICU/HarfBuzz/
FreeType/Fontconfig are runtime-loaded, with no development-header dependency.
Link `-lm -ldl -pthread` on Linux. Rectangle operations do not open native backends.
`examples/modules/layout.c` is a complete runnable example.

## Lifecycle

```c
/* unicode and catalog are application-owned ICU/Fontconfig contexts. */
mulib_glyph_font_t *face = mulib_glyph_font_match(catalog, "Noto Sans", 18);
mulib_fontshape_t *font = mulib_glyph_font_shape(face);
mulib_pglayout_fontset_t fonts = {unicode, &font, 1, "und", -1};
mulib_layout_t *layout = mulib_layout_create(&fonts);
mulib_layout_options_t options = mulib_layout_default();
mulib_layout_result_t result = {0};
mulib_xml_error_t error;
options.width = 400;
options.wrap = MULIB_LAYOUT_WRAP_WORD;
options.alignment = MULIB_LAYOUT_START;
/* Check face/layout and all return values in production code. */
mulib_layout_configure(layout, &options);
mulib_layout_set_text(layout, "Hello سلام world", strlen("Hello سلام world"));
if (!mulib_layout_compute(layout, &result, &error)) {
    mulib_layout_draw(&result, 20, 40, draw_glyph, renderer_context);
    mulib_layout_result_destroy(&result);
}
mulib_layout_destroy(layout);
mulib_glyph_font_close(face);
```

Provide an ordered array of fonts to enable fallback. All shaping fonts use
26.6 pixel scales; their sizes may differ. The first font covering the whole
grapheme is selected. Fontconfig patterns select one matched face, not an
automatic coverage-complete fallback chain. Missing coverage fails compute.

Layouts copy source bytes, the font-pointer array and language. Fonts/ICU are
borrowed; serialize calls using these native contexts. Setters are transactional.
Results own source bytes and line/glyph arrays and survive layout destruction or
later edits. They do not retain fonts; rendering requires the corresponding
font set to remain alive and unchanged. Destroy results before reusing output.

All new operations return 0 on success, -1 on failure; constructors return NULL.
Compute leaves output unchanged on failure and supplies an optional static-message
diagnostic. Backend, allocation, missing coverage and resource errors currently
share the same diagnostic category; the offset identifies the active line.

## Geometry and wrapping

* Pixels are doubles with y down. Glyph x/y locate baseline origins including
  shaping offsets. Line baseline is absolute within the result. Advances are
  logical widths, not ink extents; fontshape exposes separate glyph ink metrics.
* Each line has absolute UTF-8 `start`, `length` (excluding its hard newline),
  and `next` (including the newline). Glyphs retain absolute cluster ranges and
  font-set indices in visual order. RTL is shaped/reordered, never byte-reversed.
* ICU resolves bidi in whole-paragraph context and resets/reorders each line.
  Strong scripts and grapheme fallback are itemized before HarfBuzz shaping;
  final lines are reshaped, so joining and ligatures respect line edges.
* Width zero is unbounded. NONE honors only hard breaks. WORD uses Unicode line
  opportunities and allows an overlong word to overflow. GRAPHEME breaks only at
  extended grapheme boundaries. WORD_GRAPHEME prefers word opportunities and
  falls back to whole graphemes. An oversized single grapheme overflows.
* Spaces are preserved, including trailing spaces. Tabs use the font shaper's
  control handling, not configurable tab stops. There is no HTML whitespace
  collapse, automatic hyphenation, ellipsis, justification or vertical layout.
* CRLF, LF, CR, VT, FF, NEL and Unicode line/paragraph separators delimit lines.
  Blank lines and a final empty line after a hard break are retained. Empty input
  produces one empty line with the default font's metrics.
* START/END follow each paragraph's resolved direction; LEFT/RIGHT are physical.
  CENTER centers logical advance. For unbounded width all origins are x=0.
* Height uses maximum ascent/descent/line-gap of fonts used in that line, times
  positive spacing (default 1.2). Extra leading is split around the baseline.
  Result width is maximum content advance, not the configured constraint width.

`hit_test` clamps vertically to a line and selects the nearest visual cluster
edge horizontally. It never invents a caret inside a ligature or combining
cluster; OpenType interior carets are separately available from fontshape.
`draw` passes translated glyph copies to the callback. A nonzero callback result
stops iteration and is returned; prior renderer side effects are not rolled back.

## Limits and verification

Text is strict UTF-8 with explicit lengths, including NUL, limited to 16 MiB;
individual control glyphs still require successful shaping. Maximum 100,000
lines. Default work budgets are 100,000 shape calls and 64 MiB cumulative
source-context bytes processed by them. Whole-input context preserves bidi
embeddings across non-paragraph hard breaks. Override `MULIB_LAYOUT_MAX_MEASUREMENTS` and
`MULIB_LAYOUT_MAX_SHAPED_BYTES` before implementation inclusion. Exceeding either
fails transactionally. Candidate measurement is greedy and may reshape prefixes;
budgets bound that cost, rather than promising linear-time layout.

`tests/test_text_layout.c` exercises mixed Arabic/Latin, RTL alignment/hits,
ligatures, grapheme wrapping, hard breaks, ownership, metrics and rendering.
Set `MULIB_TEXT_REQUIRE_BACKENDS=1` to turn missing runtime backends into failures.

## Weighted rectangles

`mulib_layout_distribute` divides a finite rectangle into a row/column by
nonnegative weights, inserting fixed gaps. Nonempty weights require a positive
finite sum; available main-axis space must cover gaps. Zero weights get zero
length. Empty succeeds. This legacy API returns **1 success / 0 failure**, leaves
output unchanged on failure, and permits input/output aliasing. Coordinates use
ordinary floating-point rounding, without integer pixel snapping.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-layout.h`](../include/mulib-layout.h) |
| Implementation switch | `MULIB_IMPL_LAYOUT` |
| Repository-local includes | [mulib-glyph](mulib-glyph.md) |
| Complete example | [`examples/modules/layout.c`](../examples/modules/layout.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/layout.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
