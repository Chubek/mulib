# mulib-dyegram

[Manuals](README.md) · [Library index](index.md)

Native SVG **sequence diagrams**, using the labelled-edge source language and
owned model of [GVizz](mulib-gvizz.md). This is a focused sequence-diagram
backend, not a Mermaid/PlantUML/UML implementation.

Define `MULIB_IMPL_DYEGRAM`, `MULIB_IMPL_GVIZZ`, `MULIB_IMPL_DSDOM`,
`MULIB_IMPL_XML`, `MULIB_IMPL_STRBUF` and `MULIB_IMPL_UTF8`.
The [example](../examples/modules/dyegram.c) renders a request/reply exchange.

`mulib_dyegram_svg` borrows a graph for the call and returns an owned SVG element
or NULL for invalid input/allocation failure. Participants appear left to right
in node order. Each edge is a chronological message; reverse-direction arrows,
parallel messages and self-message loops are supported. Participant labels and
message text are literal, XML-escaped text. Empty input yields an empty labelled
canvas. GVizz's 512-participant/2048-message/1024-byte-label limits apply.

Free the SVG with `mulib_xml_destroy` and the independent source graph with
`mulib_gvizz_destroy`, in either order after rendering. Nothing is fetched or
executed. There is no mutable global state; synchronize shared graph edits.
Rendering is O(participants + messages), with fixed 180-unit participant spacing
and 70-unit message spacing; long labels may overlap. Colors use `currentColor`
for composition into HTML/CSS. SVG serialization uses the XML module.

[test_docstring.c](../tests/test_docstring.c) exercises source embedding, escaped
labels, and XML/HTML/Pandoc composition; the standalone example is also compiled
and run by the inventory checker.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dyegram.h`](../include/mulib-dyegram.h) |
| Implementation switch | `MULIB_IMPL_DYEGRAM` |
| Repository-local includes | [mulib-gvizz](mulib-gvizz.md) |
| Complete example | [`examples/modules/dyegram.c`](../examples/modules/dyegram.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dyegram.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
