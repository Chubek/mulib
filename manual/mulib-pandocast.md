# mulib-pandocast

[Manuals](README.md) · [Library index](index.md)

An output adapter from XML-compatible [DocstrIL](mulib-docstring.md#docstril)
to Pandoc's JSON AST API **1.23.1**. It does not execute Pandoc, parse Pandoc
input, or implement Pandoc's readers/writers. The standalone adapter depends
only on DSDOM and JSON, not the documentation compiler or Lua runtime.

Define `MULIB_IMPL_PANDOCAST`, `MULIB_IMPL_DSDOM`, `MULIB_IMPL_XML`,
`MULIB_IMPL_JSON`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_UTF8`, `MULIB_IMPL_PARSEC`,
`MULIB_IMPL_PARSER`, `MULIB_IMPL_PEG`, `MULIB_IMPL_AST` and `MULIB_IMPL_GLOB`.
See [the complete example](../examples/modules/pandocast.c).

## Mapping

- `docstril` becomes `{pandoc-api-version:[1,23,1],meta:{},blocks:[...]}`.
- Paragraphs, headings, code blocks, blockquotes, rules and lists use Para,
  Header, CodeBlock, BlockQuote, HorizontalRule and BulletList/OrderedList.
  Ordered lists start at one with default style/delimiter.
- Literal text splits into Str, Space and SoftBreak. Styled inlines map to
  Strong, Emph, Underline, Strikeout, Superscript, Subscript and Code.
- Links/images use Attr, inline contents and `[target,title]` destinations.
- Simple rectangular tables use Table, caption, column specs, TableHead,
  TableBody, TableFoot, Row and Cell in the modern array schema. A first row
  whose first cell has `header="true"` is the header; spans and column alignment
  are not inferred. Empty tables are permitted.
- SVG becomes RawBlock with format `html`. Figures/other extension blocks
  become Div; extension inlines become Span. Set `inline="true"` for custom
  inline elements outside ordinary paragraphs. `kbd` is recognized as inline.
- IDs/classes are preserved as Attr, other attributes as key/value pairs;
  extension wrappers also receive `data-docstril`. Code language is added as a
  Pandoc class. Metadata directives remain structured Div blocks rather than
  being guessed into Pandoc's document metadata.

## Results and limits

`from_docstril` returns a caller-owned JSON value, or NULL with an optional static
diagnostic. Input is borrowed, never changed. The root/version, element/text
profile, heading levels, list/table structure, 96-edge depth, 100000-node and
8-MiB tree-byte bounds are checked. Ragged tables fail conversion. `serialize`
returns malloc-owned compact UTF-8 JSON; its optional length excludes NUL and
is zero on failure. Free ASTs with `mulib_json_destroy` and text with `free`.
No partial result escapes. Deep JSON nesting can additionally hit JSON's own
128-container serialization bound. Conversion errors do not distinguish all
allocation, depth and schema failures.

Operations are independent and do not mutate global state. Conversion is linear
in tree bytes/nodes, aside from per-object JSON insertion. Tests cover schema,
roundtrips through a real Pandoc when available, and injected allocation failure
in [test_docstring.c](../tests/test_docstring.c),
[check_docstring.py](../tools/check_docstring.py), and
[test_hardening.c](../tests/test_hardening.c).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-pandocast.h`](../include/mulib-pandocast.h) |
| Implementation switch | `MULIB_IMPL_PANDOCAST` |
| Repository-local includes | [mulib-dsdom](mulib-dsdom.md), [mulib-json](mulib-json.md) |
| Complete example | [`examples/modules/pandocast.c`](../examples/modules/pandocast.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/pandocast.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
