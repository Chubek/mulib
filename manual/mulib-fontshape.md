# mulib-fontshape

[Manuals](README.md) · [Library index](index.md)

The default backend provides OpenType shaping through HarfBuzz.
Features, contextual substitutions/positioning, script shaping and variable-font
coordinates use the native HarfBuzz engine and the selected font's tables.

An explicit, external-runtime-free backend now executes a **bounded Latin subset**
using the new SFNT, cmap, GSUB, GPOS, GDEF, and metrics modules. It is not a
complex-script shaper or a replacement for HarfBuzz's full coverage.

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

## Native Latin implementation

Reference: the stable table layouts in **OpenType 1.9.1**. No complete OpenType
shaping conformance profile is claimed. See [the native example](../examples/fontshape-native.c)
for the complete implementation-switch recipe. Define
`MULIB_FONTSHAPE_ENABLE_NATIVE` alongside the dependency implementation switches to
compile both backends. Existing callers need no changes and `open` still selects
HarfBuzz. `open_native(path,index,scale)` and `open_memory(data,size,index,scale)`
explicitly select the native backend; memory input is copied and owned until close.
Without native enablement these new constructors return NULL.

`MULIB_FONTSHAPE_NATIVE_ONLY` enables the native backend and excludes all dynamic
loader calls. In that configuration `open(...,NULL)` also selects the native
backend; a non-NULL runtime-library path is rejected. This standalone build needs
neither HarfBuzz nor libdl:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/fontshape-native.c -o native-font
./native-font /path/to/static-font.ttf
```

The native profile is deliberately explicit:

- Static SFNT/TrueType/OpenType and TTC v1/v2 faces; no WOFF/WOFF2 decoding, named
  instances, or variable fonts. Font files are limited to 64 MiB.
- Input is printable ASCII plus embedded NUL; direction is LTR, script `Latn`,
  and language is `und` or `en`. Unsupported Unicode/scripts/options fail; there
  is no fake Arabic, Indic, combining-mark, bidi, or emoji shaping.
- The default Latin LangSys (or DFLT fallback) selects only `liga` and `kern`.
  These features default on; `liga`, `kern`, `liga=0/1`, and `kern=0/1` are the
  only accepted feature strings. Ranged features and language-specific LangSys
  selection are not implemented. Other default features are not executed.
- GSUB single substitution formats 1/2 (lookup 1), ligature format 1 (lookup 4),
  and non-recursive extension lookup 7 to those types. Ligatures retain the first
  component's original byte cluster and never turn glyph IDs back into Unicode.
- GPOS pair formats 1/2 (lookup 2), including class pairs, and non-recursive
  extension lookup 9. Scalar x/y placement and advance fields are scaled with
  checked arithmetic. Device/variation adjustments are rejected.
- Lookup flags 0 and IgnoreMarks only. GDEF glyph classes are checked and runs
  containing mark-class glyphs are rejected; this is not mark positioning.
- Without a GPOS kern feature, legacy horizontal format-0 kern pairs adjust the
  first glyph's advance. Other kern subtable profiles are rejected. This legacy
  record representation need not equal HarfBuzz's distributed kern adjustments.
- Horizontal metrics come from hhea/hmtx. Ink extents use TrueType loca/glyf header
  bounds, not outline interpretation or rasterization; CFF extents fail. GDEF
  caret lists are not interpreted: absent caret data succeeds with zero carets,
  present data reports failure instead of inventing positions. `native()` is NULL.

Runs are limited to 65,536 input bytes and 128 lookups per selected feature.
A 32-MiB byte-weighted lookup-work budget bounds adversarial processing; budget
exhaustion returns failure without publishing a partial run. Output ownership and
0/-1 failure-atomic conventions are identical to the default backend.

### Allocation and verification

Define compatible `MULIB_FONTSHAPE_MALLOC`, `MULIB_FONTSHAPE_CALLOC`, and
`MULIB_FONTSHAPE_FREE` hooks in the implementation TU to replace libc allocation.
Native file/staging buffers additionally use the typecore allocator hooks.
`run_destroy`, `close`, and the new `carets_destroy` use the configured deallocator;
existing callers may still `free` carets with the default allocator.

`tests/test-fontshape-native.c` uses an independently constructed synthetic font
for deterministic glyph, ligature, kerning, scaling, TTC, ownership, malformed
input, and unsupported-script checks. `tests/test-font-layout.c` checks lookup
execution; `tests/test-font-allocations.c` injects failure at every allocation in
the tested construction/shaping/escaping paths. `tests/test_fontshape_differential.c`
compares Latin glyph IDs, byte clusters, advances, and offsets against HarfBuzz
with features outside this profile disabled. It explicitly skips when test fonts
or HarfBuzz are unavailable; it is not a complex-script conformance suite.
`tests/fuzz_publishing_fonts.c` is the bounded native-only libFuzzer target.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-fontshape.h`](../include/mulib-fontshape.h) |
| Implementation switch | `MULIB_IMPL_FONTSHAPE` |
| Repository-local includes | [`include/detail/mulib-fontshape-native.h`](../include/detail/mulib-fontshape-native.h), [mulib-dynload](mulib-dynload.md), [mulib-fontcmap](mulib-fontcmap.md), [mulib-fontgdef](mulib-fontgdef.md), [mulib-fontgpos](mulib-fontgpos.md), [mulib-fontgsub](mulib-fontgsub.md), [mulib-fontkern](mulib-fontkern.md), [mulib-fontmetrics](mulib-fontmetrics.md), [mulib-sfnt](mulib-sfnt.md), [mulib-utf8](mulib-utf8.md) |
| Complete example | [`examples/modules/fontshape.c`](../examples/modules/fontshape.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/fontshape.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
