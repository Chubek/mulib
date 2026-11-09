# mulib-xtas

[Manuals](README.md) · [Library index](index.md)

An allocation-free, SAX-style stream over the [documentation DOM](mulib-dsdom.md).
Define `MULIB_IMPL_XTAS`, `MULIB_IMPL_DSDOM`, `MULIB_IMPL_XML`,
`MULIB_IMPL_STRBUF` and `MULIB_IMPL_UTF8`. The [example](../examples/modules/xtas.c)
counts events.

`mulib_xtas_stream` emits ENTER, descendant events, then LEAVE for each element.
TEXT carries literal UTF-8. CDATA becomes TEXT, comments are skipped, and XML
document wrappers do not emit events. Root element depth is zero; text below it
has depth one. ENTER/LEAVE expose the element name and all attributes through
the borrowed `event.node` and `mulib_xml_attr_*` functions.

The callback returns nonzero to continue. Stream returns 1 on completion, 0 on
callback cancellation, and -1 for NULL arguments or nesting beyond XML's limit.
Already emitted events are not rolled back after cancellation/error. Empty
elements emit ENTER and LEAVE. A text-only subtree emits one TEXT event.

Events, node pointers and strings are borrowed; copy anything retained beyond
the callback. Do not mutate/destroy the tree or reenter it from the callback.
There is no global state and independent trees can be streamed concurrently.
Traversal is O(nodes), with bounded recursive stack use. To retain an owned
stream, use [TAS](mulib-tas.md). Tests exercise completion, cancellation and
roundtrip composition in [test_docstring.c](../tests/test_docstring.c).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-xtas.h`](../include/mulib-xtas.h) |
| Implementation switch | `MULIB_IMPL_XTAS` |
| Repository-local includes | [mulib-dsdom](mulib-dsdom.md) |
| Complete example | [`examples/modules/xtas.c`](../examples/modules/xtas.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/xtas.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
