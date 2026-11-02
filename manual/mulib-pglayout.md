# mulib-pglayout

An owned page-object model with stylesheet-driven block flow, Unicode-aware
text wrapping, pluggable measurement and JSON/YAML persistence.

## Integration

Define all switches before including `mulib-pglayout.h`:

```
MULIB_IMPL_PGLAYOUT MULIB_IMPL_STYLESHEET MULIB_IMPL_CSS MULIB_IMPL_XML
MULIB_IMPL_STRBUF MULIB_IMPL_UTF8 MULIB_IMPL_PARSEC MULIB_IMPL_JSON
MULIB_IMPL_YAML MULIB_IMPL_UNIBREAK MULIB_IMPL_BIDI MULIB_IMPL_UNIPROC
MULIB_IMPL_FONTSHAPE MULIB_IMPL_DYNLOAD
```

Link `-lm -ldl` where required. No native development headers or link-time
third-party dependency is required. ICU is needed when laying out nonempty text;
HarfBuzz is needed only if the supplied native font-measure adapter is used.
`examples/modules/pglayout.c` additionally uses `MULIB_IMPL_SYSFONTS` to select
a system font. That example computes layout and round-trips YAML.

## Objects and ownership

`new(kind)` creates a detached owned object:

* `LAYOUT`: page/group containing vertically flowing child blocks.
* `TEXTBLOCK`: a semantic text-group container with the same block-flow rules.
* `TEXT`: a UTF-8 text leaf, wrapped into measured line slices.
* `IMAGE`: a copied URI and intrinsic dimensions; no image fetching/decoding.
* `PATH`: copied polyline/polygon coordinates and a closed flag. Coordinates are
  finite, nonnegative and local to the item; maxima define intrinsic dimensions.

`append` transfers a detached child to a layout/textblock only on success. It
rejects cycles, duplicate attachment and nesting beyond 60 items. `destroy`
releases a root/detached subtree; attached items are parent-owned and direct
destroy calls are ignored. Children, XML selector metadata, content, points,
computed styles and lines have borrowed accessors; do not mutate/destroy borrowed
objects. `attr` copies id/class/style or other XML-compatible selector metadata.
`content`, `image_size` and `path` copy validated source data transactionally.
Input text is strict NUL-terminated UTF-8 with at most `INT32_MAX` bytes.
Geometry/point dimensions are bounded by 1,000,000 pixels.

## Layout and measurement

`compute(root, sheet, unicode, available_width, measure, context)` requires a
detached root and positive finite width <=1,000,000. At most 100,000 items are
processed. Styles inherit through the tree. Containers place children vertically;
margins do not collapse. CSS width/height are **content** dimensions; padding is
included in each returned border box, margin is outside it. Auto container/text
width fills available space. Images/paths use intrinsic width when auto; images
with explicit width and auto height preserve their intrinsic aspect ratio.
Explicit height may allow content overflow. Hidden subtrees consume no space and
have zero boxes/line counts. No clipping, rendering or pagination is implicit.

The measure callback receives a UTF-8 candidate slice and computed style and
returns finite, nonnegative pixel advance, or -1 on failure. It must not mutate
the tree. It can select/cache fonts from family/size and perform script/fallback
itemization. `font_measure` is a convenience adapter for one fixed-size HarfBuzz
font, using bidi visual runs; the application must select the font scale to match
the style. It assumes one script per directional run, guesses otherwise, and
does not supply script/fallback itemization or paragraph-preserving line bidi.

### Paragraph-aware font-set shaping

`mulib_pglayout_fontset_t` borrows an ordered array of HarfBuzz fonts, an ICU
context, language, and base direction (0/1/-1/-2 as in bidi). All fonts must be
scaled to the same intended size in 26.6 pixels. No system font lookup or file
I/O occurs implicitly; use sysfonts to choose/load faces explicitly.

`shape_line(fonts, paragraph, bytes, start, length, &line)` resolves the complete
paragraph, extracts the requested line with ICU's paragraph-aware L1/L2 rules,
and itemizes directional runs into script/font items at extended grapheme
boundaries. A grapheme uses the first font covering all its non-control,
non-default-ignorable scalars. Common/Inherited graphemes take an adjacent strong
script (preceding preferred). Adjacent equal-script/equal-font items are combined
for contextual shaping; items within RTL runs are emitted in visual order.
Missing coverage or a shaped missing glyph fails explicitly. Source ranges must
be scalar/grapheme aligned and cannot cross paragraph separators.

The result owns visual runs and glyph arrays. Each run has font-set index,
resolved script and level, paragraph-absolute source range and glyph clusters,
plus positioned glyphs and total pixel advance. Use `shaped_line_destroy` to
free it. Font objects are borrowed, never owned or cached by the result.
This is Unicode Script-property itemization with adjacent neutral resolution,
not a full Script_Extensions/paired-script tailoring engine. Sequence support
and color rendering still depend on native fonts/backends; ordered fallback
does not implement language-specific font preference or synthesize glyphs.

`compute_shaped(root, sheet, fonts, font_size, width)` applies the same shaping
path to wrapping and retains owned final-line glyph runs. Every visible text
style must match `font_size`; callers must configure the fonts at `font_size*64`.
The current explicit font-set path is fixed-size; applications requiring several
sizes can supply custom measurement or separate styled groups. No size mismatch
is silently ignored. `shaped_line(item, index)` borrows the cached final result,
with offsets relative to that leaf's text. Mutation invalidates these accessors;
a successful ordinary compute clears shaping caches. Failed compute preserves
old boxes, line pointers and glyph caches. Keep fonts alive through rendering.

For drawing, iterate cached visual runs left-to-right, select the corresponding
fontdraw face/size, render each glyph id, apply x/y offsets, bearings and advances,
and composite the returned bitmap. There is no implicit baseline/font ascent
policy, image loading, GPU atlas allocation or raster compositor in pglayout.

### Wrapping and geometry

Unicode line boundaries are considered greedily until the first overflowing
candidate or mandatory break. With nonmonotonic custom measurement, a later
candidate might fit but is not searched beyond that first overflow. Overlong segments
fall back to extended-grapheme boundaries; a single overwide grapheme overflows
rather than being split. Hard CR/LF/CRLF/NEL/LS/PS/VT/FF breaks are consumed but
excluded from returned line slices. Blank/trailing lines are preserved. Other
whitespace, including trailing spaces, is preserved and measured. There is no
CSS whitespace collapsing, hyphenation, justification, inline span coalescing,
floating boxes, tables, flex/grid layout or automatic multipage fragmentation.
This is an explicit block-flow profile, not complete Pango/browser layout.

Each line records its original UTF-8 byte start/length and top-left pixel box.
Line height is font-size times the style multiplier, not queried font ascent.
Alignment moves the line within the content width. Draw with an application
baseline policy and font metrics. Ordinary compute stores source slices/geometry;
compute_shaped additionally owns final glyph arrays until recompute/destruction,
not a raster atlas or image/path renderer.

Layout uses scratch results and commits only after complete success; failure
preserves prior geometry. Any source mutation invalidates geometry for the entire
root. `geometry` returns -1 until recomputed; `lines` then returns NULL/count zero.
Line fitting remeasures candidate prefixes, so long text can have quadratic
measurement work; callbacks should cache reusable shaping data when needed.
`MULIB_PGLAYOUT_MAX_MEASUREMENTS` limits candidate measurements per compute to
1,000,000 by default (override consistently in the implementation TU). Exhaustion
fails transactionally. This bounds call count, not the time/memory of an individual
application callback. A callback that returns success without writing a finite
advance is rejected. Auto image-height scaling uses extended intermediates and
rejects unrepresentable geometry; an explicit height bypasses aspect-ratio scaling.

## Persistence

`to_json` produces an owned DOM; `from_json` validates and constructs an owned
tree. `serialize(item, yaml)` returns malloc-owned JSON (0) or YAML (1);
`deserialize` parses the selected format then validates the same schema.

```
{"version":1,"root":{"type":"layout","attrs":{},"children":[
  {"type":"text","attrs":{"class":"title"},"content":"Hello"}
]}}
```

Every node requires `type` and `attrs`. Containers require `children`, text
requires `content`, images require `content`, `width`, `height`, and paths require
`points` (flat x/y numeric array) and `closed` (boolean). Unknown fields, missing
fields, wrong types, embedded-NUL names/content, bad dimensions and excessive
depth/count fail. Tree nesting is limited to 60 to remain within the underlying
JSON/YAML container depth limits. The YAML grammar is exactly `mulib-yaml`'s
documented subset. No objects, URIs or callbacks are executed while loading.

Persistence stores the source model, including selector metadata and intrinsic
geometry. Computed boxes/lines and external sheets/fonts/measurement callbacks
are not serialized; recompute them after loading. Return NULL for parsing,
schema, depth or allocation errors. Buffers/DOMs use `free`/`mulib_json_destroy`.

## Validation

`tests/test_pglayout.c` covers cascade, invalid styles, ownership, source-model
round trips, wrapping, mandatory breaks, hidden flow and transactional failure.
`tests/test_text_allocations.c` injects allocation failures. Run
`python3 tools/check_text.py --sanitizers --require-backends` for focused checks.
