# mulib-stylesheet

[Manuals](README.md) · [Library index](index.md)

Typed, validated layout styles and author-rule cascading over MuLIB's existing
CSS parser and XML selector model. `mulib-pglayout` uses these styles directly.

## Integration

Define `MULIB_IMPL_STYLESHEET`, `MULIB_IMPL_CSS`, `MULIB_IMPL_XML`,
`MULIB_IMPL_STRBUF`, and `MULIB_IMPL_UTF8` before including the header.
No native runtime is needed. Example: `examples/modules/stylesheet.c`.

`parse(source, length, error)` owns the resulting sheet and validates every
selector, property and value, even in rules that match no nodes. Syntax errors,
unsupported values/properties and OOM return NULL. `destroy` releases a sheet.
Error messages are static; parser errors preserve byte positions, semantic
validation errors use offset zero.

## Supported grammar

Selectors are those in `mulib-css`: type, universal, id, class, attribute
presence/equality, descendant/child combinators and comma groups. At-rules,
pseudo-classes/elements, sibling combinators and selector escapes are rejected.
Declarations use the existing parser's comments and `!important` handling.

| Property | Values | Default |
|---|---|---|
| `font-size` | Positive decimal, optionally `px` | 16 |
| `line-height` | Positive unitless multiplier | 1.2 |
| `width`, `height` | Nonnegative decimal, optionally `px`, or `auto` | auto |
| `margin`, `padding` | One nonnegative decimal, optionally `px` | 0 |
| `color`, `background-color` | `#rgb`, `#rgba`, `#rrggbb`, `#rrggbbaa`, black, white, transparent | black / transparent |
| `text-align` | left, center, right | left |
| `display` | block, none | block |
| `font-family` | Unquoted UTF-8 family name, at most 127 bytes | sans |

Numbers are parsed locale-independently and bounded by 1,000,000. Every decimal
point must be followed by a digit (`.5` works, `1.` does not). No exponents,
negative lengths, percentages, relative units, expressions, multi-value box
shorthands, quoted/fallback family lists, custom properties or CSS-wide keywords
are accepted. Property names and keywords are ASCII case-insensitive; selectors
retain the CSS module's case-sensitive matching. Colors use `0xRRGGBBAA`.
The profile is intentionally explicit rather than silently dropping unknown CSS.

## Resolution

`resolve(sheet, node, parent_style, &style, error)` borrows an XML element and
optional parent computed style. The sheet may be NULL. Font size, family,
line height, color and text alignment inherit; other properties start at their
defaults. Inherited font size/line height must be finite, positive and <=1,000,000;
alignment and the terminated UTF-8 family buffer are validated before use.
The resolver applies matching rules and then the element's `style`
attribute according to importance, inline specificity, selector specificity
(ids/classes+attributes/types), and source order. Matching comma branches are
ranked independently; a nonmatching high-specificity branch does not promote a
matching low-specificity branch. Later equal-rank declarations win.

The result is a value struct with no owned pointers; family text is copied into
its fixed buffer. `width`/`height` -1 denote auto. Return 0/-1, preserving output
on failure. Public parent styles should be initialized with `default` or a prior
successful resolve. No stylesheet state is mutated by resolution.

This API handles author rules only, not browser UA/user origins, animations,
media queries, custom properties or a full CSS computed-value system. Layout
interprets lengths; the resolver does not measure text or fetch fonts.

Tests: `tests/test_pglayout.c`, allocation failure tests and standalone examples.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-stylesheet.h`](../include/mulib-stylesheet.h) |
| Implementation switch | `MULIB_IMPL_STYLESHEET` |
| Repository-local includes | [mulib-css](mulib-css.md) |
| Complete example | [`examples/modules/stylesheet.c`](../examples/modules/stylesheet.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/stylesheet.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
