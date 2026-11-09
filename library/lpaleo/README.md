# lpaleo

Auto-loaded Mune profiles for [`mulib-paleography.h`](../../include/mulib-paleography.h).
`lpaleo.define` copies vector glyphs into the native inventory. `lpaleo.native`
explicitly calls the host's configured outline provider. `lpaleo.script` adds a
longest-match transcription profile. `shape` returns glyph tokens and direction;
`transliterate` concatenates those tokens.

Included `identity`, `elder-futhark` and `greek` profiles are Unicode mappings,
not authenticated reconstructions of historical handwriting. Custom profiles
can name native/vector glyphs and use ltr/rtl/ttb/boustrophedon layout.

```lua
lpaleo.define{name='sun', codepoint=0xE010, advance=700,
  path='M 0 0 L 600 0 L 300 700 Z'}
lpaleo.script('imaginary', {direction='rtl', mapping={a='sun'}})
```

SVG path output, legacy SVG font resources, CSS `@font-face`, and PGF/fontspec
LaTeX packages/8-bit PostScript encodings share the same glyph definitions.
No fonts, files or programs are implicitly loaded. See the
[manual](../../manual/mulib-paleography.md) for formats and limitations.

Edit `lpaleo.lua` and regenerate with `python3 tools/embed_notation.py`.
