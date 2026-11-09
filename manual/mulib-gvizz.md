# mulib-gvizz

[Manuals](README.md) · [Library index](index.md)

A small directed labelled graph model and original SVG renderer. It does not
invoke Graphviz or parse DOT; [mulib-graphviz](mulib-graphviz.md) is the separate
DOT writer. Define `MULIB_IMPL_GVIZZ`, `MULIB_IMPL_DSDOM`, `MULIB_IMPL_XML`,
`MULIB_IMPL_STRBUF` and `MULIB_IMPL_UTF8`. No third-party library is needed.
The [example](../examples/modules/gvizz.c) prints SVG.

## Source language

```text
Parser: Read source
Renderer: Write output
Parser -> Renderer: DocstrIL
Renderer -> Renderer: Repeat
```

Each nonblank line is `ID: label` or `ID -> ID` with optional `: label`.
Leading/trailing spaces/tabs and CRLF are accepted. Lines whose first nonspace
character is `#` are comments. IDs are ASCII letters/underscore followed by
letters, digits, underscore or hyphen, at most 63 bytes. Labels are literal
XML-valid UTF-8, at most 1024 bytes, with no markup expansion. Colons after the
first label delimiter belong to the label. Labels cannot contain newlines in
the parsed language; the direct API accepts valid multiline labels as text.

Edges implicitly create missing participants in first-use order. A declaration
relabels an existing node. Multiple edges, loops and empty graphs are accepted.
There are at most 512 nodes, 2048 edges and 1 MiB of source. Malformed input,
limits and allocation failures return NULL with a static diagnostic at the
start of the offending line. This compact parser does not distinguish every
allocation failure from a syntax/limit failure.

## Ownership and rendering

`create`/`destroy` manage an independent graph; destroy is NULL-safe. Direct
`node` insertion requires a unique ID and copies both strings. `edge` requires
existing IDs and copies its label. Both return 1/0 and leave the graph unchanged
on failure. Indexed names/labels/edge records are borrowed; edge insertion can
invalidate edge-record pointers, and destruction invalidates all borrows.

`svg` returns an owned SVG element tree, freed with `mulib_xml_destroy`; NULL
means invalid graph/allocation failure. Serialize with `mulib_xml_serialize`.
The deterministic square grid draws boxes, directional arrows, labels and self
loops. It is not an automatic crossing minimizer or text-shaping engine: long
labels/parallel edges can overlap. Arrowheads have no globally colliding marker
IDs. Rendering is O(nodes + edges); insertion/ID lookup is linear in nodes.
Callers synchronize access. See [Docstrings tests](../tests/test_docstring.c),
including graph embedding and [allocation failure checks](../tests/test_hardening.c).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-gvizz.h`](../include/mulib-gvizz.h) |
| Implementation switch | `MULIB_IMPL_GVIZZ` |
| Repository-local includes | [mulib-dsdom](mulib-dsdom.md) |
| Complete example | [`examples/modules/gvizz.c`](../examples/modules/gvizz.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/gvizz.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
