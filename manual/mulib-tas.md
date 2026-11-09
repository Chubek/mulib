# mulib-tas

[Manuals](README.md) · [Library index](index.md)

An owned documentation token stream: OPEN, ATTRIBUTE, TEXT, CLOSE. It snapshots
the XML-compatible [documentation DOM](mulib-dsdom.md) without borrowing the
original nodes. Define `MULIB_IMPL_TAS`, `MULIB_IMPL_XTAS`, `MULIB_IMPL_DSDOM`,
`MULIB_IMPL_XML`, `MULIB_IMPL_STRBUF` and `MULIB_IMPL_UTF8`.
See the [runnable example](../examples/modules/tas.c).

## Lifecycle and grammar

`create` returns an empty owned stream or NULL. `push` copies a token and
returns 1 on success, 0 for invalid fields, the 1,000,000-token bound or allocation
failure. Failure leaves the stream unchanged. OPEN/CLOSE require XML-valid names
and NULL values; ATTRIBUTE requires a name and value; TEXT requires NULL name
and non-NULL XML-valid UTF-8 value. Empty text/attribute values are allowed.
`at` returns a borrowed token or NULL out of range. Push can invalidate token
pointers, though independently allocated string storage is retained until
destroy. `destroy(NULL)` is harmless.

`from_dom` snapshots in traversal order, dropping comments and normalizing CDATA
to TEXT. XML document wrappers produce no tokens. It returns NULL on invalid
arguments, depth/token overflow or allocation failure; no partial stream escapes.

`to_dom` validates exactly one root and matching OPEN/CLOSE names. ATTRIBUTE
tokens must immediately follow their OPEN or another ATTRIBUTE for that OPEN;
duplicate attribute names, text outside the root, empty streams, extra roots and
unbalanced nesting fail. The nesting bound is `MULIB_XML_DEPTH_LIMIT` elements.
Success returns an owned element root. Failure returns NULL and an optional
static diagnostic; `error.offset` is the offending token index, not a byte offset.
No partial DOM escapes. Free trees with `mulib_xml_destroy`.

These conversions preserve element names, attributes and literal text, not XML
lexical choices such as CDATA versus entity escaping. Traversal is linear except
XML attribute insertion's per-element scans. Independent streams need no shared
state; callers synchronize edits. Regression evidence includes roundtrips,
invalid attribute placement/names, cancellation of the underlying event stream,
and allocation failures in [test_docstring.c](../tests/test_docstring.c) and
[test_hardening.c](../tests/test_hardening.c).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-tas.h`](../include/mulib-tas.h) |
| Implementation switch | `MULIB_IMPL_TAS` |
| Repository-local includes | [mulib-xtas](mulib-xtas.md) |
| Complete example | [`examples/modules/tas.c`](../examples/modules/tas.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/tas.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
