# Terminal styling

[Manuals](README.md) · [Library index](index.md)

Include `mulib-termstyle.h`; define `MULIB_IMPL_TERMSTYLE`, `MULIB_IMPL_TERMINFO`,
`MULIB_IMPL_STRBUF`, `MULIB_IMPL_CSS`, `MULIB_IMPL_XML`, `MULIB_IMPL_UTF8`,
`MULIB_IMPL_VTERM` and `MULIB_IMPL_GRID` in one TU. These additional dependency
switches supply the CSS/shared-cell extension; existing indexed-context APIs
remain source/behavior compatible. No curses/terminfo runtime library is linked.
[Example](../examples/modules/termstyle.c).

Initialize a context with `mulib_termstyle_init(&style, &info, enabled)`. The
terminfo record is borrowed and must remain alive. NULL info forces plain mode.
The caller decides TERM database loading, isatty and NO_COLOR policy. ANSI output
is never inferred: call `mulib_terminfo_ansi` explicitly if that profile is wanted.

A `mulib_termstyle_spec_t` contains attribute flags BOLD, DIM, ITALIC, UNDERLINE,
REVERSE and foreground/background indexed colors. Set each color to -1 for
terminal default (a zero-initialized spec instead requests color zero).
`apply` emits sgr0, then requested capabilities and colors. `reset` emits sgr0.
`text` emits apply + literal text bytes + reset; it can safely borrow text from
the output buffer. It does not escape control characters in application text.

Enabled styling requires the capabilities actually requested (`bold`, `dim`,
`sitm`, `smul`, `rev`, `setaf`, `setab`) and `sgr0`. Color indexes must be smaller
than the record's `colors` capability. Unsupported/missing capabilities, malformed
expansion, invalid specs and OOM return 0 without appending partial escapes.
Disabled contexts emit no escapes and text is copied unchanged. Successful calls
return 1 and append to an initialized `mulib_strbuf_t`; free it normally.

The legacy context API does not approximate truecolor. No attribute state
tracking, nested-style restoration, terminal probing, or terminfo `ncv`
combination policy is implemented. Callers
must choose combinations supported by their terminal. Expansion uses
[terminfo's documented bounded subset](mulib-terminfo.md).

## CSS and shared terminal styles

The new APIs return **zero on success**, with negative EINVAL/ENOMEM/ENOTSUP on
failure (unlike the preserved legacy APIs' 1/0 convention):

* `mulib_termstyle_resolve(sheet, node, parent, output, error)` uses
  `mulib_css_cascade` with the element's optional inline `style` attribute.
* `mulib_termstyle_from_css(declarations, parent, output, error)` applies an
  already cascaded list, respecting duplicate-property importance through
  `mulib_css_get`. A NULL declaration list is empty.
* `mulib_termstyle_emit(style, info, flags, output)` emits a complete shared
  `mulib_term_style_t` rendition.
* `mulib_termstyle_write(style, info, flags, text, length, output)` safely emits
  UTF-8 source text between a rendition and default rendition/color reset.

All outputs are transactional. Parent/default colors and attributes are
inherited as terminal rendition; this is a terminal CSS profile, not browser
painting/layout. Shared styles can be used directly by vterm, TUI and curses.
Supported properties/values are:

| Property | Values |
| --- | --- |
| `color`, `background-color` | `#rgb`, `#rrggbb`, integer `rgb(r,g,b)`, the 16 basic CSS color names, `grey`, `orange`, `rebeccapurple`, `currentColor` |
| `background-color` additionally | `transparent` preserves the parent terminal background |
| `font-weight` | `normal`, `bold`, 100 through 900 in steps of 100 (600+ is bold) |
| `font-style` | `normal`, `italic`, `oblique` |
| `text-decoration` | `none`, or whitespace-separated `underline`, `line-through`, `blink` |
| `visibility` | `visible`, `hidden` (conceal attribute) |
| `--term-inverse`, `--term-dim`, `--term-blink` | `on`, `off` |

`inherit`/`initial`/`unset` are supported for each property. Initial means terminal
default colors/cleared bits; unset inherits except for background-color, whose
initial is terminal default. Missing properties preserve parent rendition.
Ordinary property/value keywords are ASCII case-insensitive; custom property
names remain exact. Unknown properties or winning values fail with a static
diagnostic. No alpha colors, gradients, layout, var(), shorthand expansion,
browser font selection or full CSS color-name database is promised.

NULL terminfo selects **plain** output; no TERM/environment detection or implicit
ANSI occurs. Explicitly call `mulib_terminfo_ansi` or load a capability record to
enable styling. `MULIB_TERMSTYLE_PLAIN` overrides styling. `emit` starts with both
`sgr0` and `op`, because attribute reset does not necessarily reset colors.
Required missing capabilities cause failure rather than partial output.

RGB is approximated to the advertised standard 8/16/256-color palette by nearest
RGB distance. Actual terminal palettes can differ; there is no palette query or
gamma correction. `MULIB_TERMSTYLE_TRUECOLOR` explicitly permits ANSI 24-bit
sequences and is accepted only with the explicit ANSI profile. Strike-through
also requires that profile; other attributes use named terminfo capabilities.
Style values and flag bits are validated even in plain mode.

`write` rejects invalid UTF-8 and replaces C0 except LF/TAB, DEL and C1 scalars
with U+FFFD. This prevents source text from injecting escape sequences. Legacy
`text` deliberately retains its literal-byte behavior; do not use it for
untrusted editor text. Neither API performs terminal I/O or restores an arbitrary
prior style; write resets to terminal defaults at the end.

The [syntax backend](mulib-termsyn.md) connects these styles to editor-buffer
token classes and incremental per-line lexical states.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-termstyle.h`](../include/mulib-termstyle.h) |
| Implementation switch | `MULIB_IMPL_TERMSTYLE` |
| Repository-local includes | [mulib-css](mulib-css.md), [mulib-terminfo](mulib-terminfo.md), [mulib-vterm](mulib-vterm.md) |
| Complete example | [`examples/modules/termstyle.c`](../examples/modules/termstyle.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/termstyle.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
