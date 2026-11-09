# mulib-paleography

[Manuals](README.md) · [Library index](index.md)

A hybrid script toolkit: native vector-glyph storage/font-provider callbacks plus
automatically loaded [lpaleo Lua profiles](../library/lpaleo/README.md). It emits
SVG path text, legacy SVG fonts, CSS web-font declarations, LaTeX PGF/fontspec
packages and companion PostScript encodings. It does **not** fabricate OpenType
fonts, decipher scripts, implement scholarly text criticism or certify historical
glyph authenticity. Included historical profiles are Unicode transcription maps.

## Integration and context

Define PALEOGRAPHY, MUNE, JSON, SVG, BASE64, TEX and their transitive switches once.
The [example](../examples/modules/paleography.c) uses the complete explicit
[notation recipe](../examples/notation-common.h). Link math/threads/dynamic loader
as required by Mune. No native font engine or third-party library is implicitly
opened. A host may explicitly adapt an OS/MuLIB font service through the callback.

`create` copies all option strings, creates a restricted independent Mune state,
and auto-loads lpaleo. Defaults: id/family Paleo, 1000 units/em, ascent 800,
descent 200. ID is <=63 ASCII letters (safe TeX command/package stem); family is
<=127 UTF-8 bytes. Units/em are 16..16384; metrics are finite/nonnegative with a
positive total <=4 em. Native font names reject TeX command/delimiter injection;
web font URLs/formats are copied and quoted, not fetched. Explicit callback
userdata is borrowed until context destruction. `destroy(NULL)` is harmless and
is a no-op during an active operation. Contexts cannot be reentered and require
caller synchronization. The Mune accessor is borrowed; do not destroy its state.

## Glyphs and native calls

`define` copies a unique ASCII glyph name, scalar, advance and optional outline.
Names are letters followed by letters/digits/underscore, <=63 bytes. Scalar zero
selects an unused BMP Private Use Area scalar. Other scalars must be XML-valid,
non-control Unicode and unique in the inventory. Advance is 0..4 em in font units.
There are <=4096 glyphs and <=8 MiB of copied glyph names/path data.

Outlines use explicit **absolute M, L, Q, C, Z** commands with finite signed
decimal coordinates in +/-1000000, separated by whitespace/commas. Every segment
must name its command; relative/shorthand/arc/exponent syntax is rejected.
Line/curve/close requires an open contour. Paths are <=64 KiB/4096 commands.
Coordinates are y-up font units. Empty paths are native-font fallback glyphs,
not invented outlines. Registration returns 1/0 and is atomic on allocation or
validation failure. Borrowed glyph descriptors remain stable until destruction.

`import` invokes the explicitly configured native font callback with font/scalar.
The callback appends an outline to a borrowed strbuf, supplies advance, and returns
nonzero. The library copies and validates it. The host owns path authorization,
backend choice and callback allocation limits; buffers are checked after return.
No font files are read automatically. `lpaleo.native(name,scalar,font)` invokes the
same native service; `lpaleo.define{...}` registers vector glyphs from Lua.

## Lua transcription and layout

Global lpaleo is present immediately after creation; `require('lpaleo')` also
works. `extend` runs <=1 MiB chunks, preserving preceding registration/Lua side
effects on error. Its diagnostics own their text. `script(name,{mapping=...,
direction=...,lowercase=...})` copies a profile; mapping keys/values are nonempty
strings <=64 bytes. Longest matching key wins. Unmatched input advances by one
UTF-8 scalar. Lowercasing is the Lua ASCII string operation, not Unicode folding.

`identity`, `elder-futhark` and `greek` are included. They are convenient Unicode
maps, not universal transliteration standards, phonological models or original
historical font designs. Invented profiles can map tokens to vector glyph names.
`shape` returns glyph tokens and direction; `transliterate` concatenates tokens.

`svg` runs a profile on <=1 MiB XML-valid input and <=16384 output tokens. It
returns an independent owned SVG element or NULL. Registered vector glyphs render
as scaled paths. Known scalars without paths use literal native SVG text with the
configured CSS family; font availability/coverage is not guaranteed. Unknown
multi-character glyph names fail. Spaces advance, CR is ignored, LF breaks lines.
Layout supports ltr, rtl, ttb and alternating boustrophedon lines; font size is
(0,1000], line height [.1,10] em, with zero selecting 32/1.4 defaults. ttb is a
simple column progression. There is no OpenType shaping, Unicode bidi, script
itemization, automatic mark positioning, word wrapping or pagination.

## Font and LaTeX outputs

- `font` returns an owned SVG font resource with units/ascent/descent, Unicode
  mappings, advances and vector glyph paths. Outline-less glyphs are omitted,
  not exported as invisible substitutes. SVG fonts are **legacy** and are not
  supported as web fonts by every modern browser. `svg` path output is the
  portable visual fallback.
- `css` returns malloc-owned `@font-face`: supplied URL with woff2/woff/opentype/
  truetype/svg format, or a base64-embedded SVG font when no URL is provided.
  Embedded SVG URLs explicitly reference the generated font ID.
  Supply a real WOFF/OTF asset for ordinary modern browser font loading. This
  module does not convert outlines to WOFF, TTF or OTF and never fetches the URL.
- `package` returns a LaTeX `.sty` hybrid. Vector glyphs use PGF outlines and
  advances/baselines; quadratics are converted to cubic PGF segments. A
  fixed-decimal dimension formatter avoids scientific notation unsupported by TeX.
  The consumer's TeX dimension limits and scaled-point precision still apply. An optional
  native font uses fontspec on LuaLaTeX/XeLaTeX. Macros `\Paleoglyph{name}` and
  `\Paleotext{text}` use the configured ID prefix. Unicode declarations are
  emitted when available. PGF/iftex/fontspec are **consumer LaTeX packages**, not
  linked library dependencies; no TeX compiler runs inside MuLIB.
- `encoding` returns a 256-slot PostScript `.enc`: .notdef then <=255 registered
  glyph names, with unused slots filled. Larger inventories fail explicitly.
  This is a companion encoding, not a Type1/TFM/OpenType font generator.
- `latex` transcribes source to package glyph/native text commands. Unknown or
  outline-less glyphs require a configured native font. Output remains in source
  token order; use the SVG layout for explicit directional/vector placement.

All returned strings are malloc-owned, NUL-terminated, with optional lengths
excluding NUL and zero on failure. SVGs are freed with `mulib_xml_destroy` and
remain valid after context destruction. No partial output escapes. Diagnostics
copy source offsets/messages. Output can be larger than the glyph byte budget due
to escaping, PGF expansion and base64. CSS/path semantics and font licensing are
the caller's responsibility; this is not a hardened sandbox or sanitizer.

Mune independently bounds heap/work/calls/results. Default states remove file/OS/
package/debug/loading capabilities; the host can explicitly add native bindings.
Evidence: [functional/native callback tests](../tests/test_notation.c),
[allocation tests](../tests/test_notation_alloc.c),
[SVG/LaTeX interoperability](../tools/check_notation.py), and
`python3 tools/embed_notation.py --check`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-paleography.h`](../include/mulib-paleography.h) |
| Implementation switch | `MULIB_IMPL_PALEOGRAPHY` |
| Repository-local includes | [mulib-base64](mulib-base64.md), [mulib-json](mulib-json.md), [`include/mulib-mune-markup.inc`](../include/mulib-mune-markup.inc), [mulib-mune](mulib-mune.md), [`include/mulib-paleography-data.inc`](../include/mulib-paleography-data.inc), [mulib-svg](mulib-svg.md), [mulib-tex](mulib-tex.md) |
| Complete example | [`examples/modules/paleography.c`](../examples/modules/paleography.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/paleography.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
