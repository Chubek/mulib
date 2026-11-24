# `mulib-prettysexpr.h`: portable document interchange

`mulib_pretty_parse_expr` consumes one complete NUL-terminated document
expression (plus optional whitespace), not programming-language source.
On success it stores an owned document and its root ID in the two output
parameters. Release the document with `mulib_prettydoc_free`. It returns zero
and leaves both outputs unchanged on failure.

`mulib_pretty_render_expr` parses and renders in one call. It returns a
malloc-owned NUL-terminated string to release with `free`, or `NULL` on failure.
See [PrettyDoc](mulib-prettydoc.md#input-format) for the grammar and limits.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-prettysexpr.h`](../kernels/mulib-prettysexpr.h) |
| Implementation switch | `MULIB_IMPL_PRETTYSEXPR` |
| Repository-local includes | [mulib-prettyemit](mulib-prettyemit.md) |
| Complete example | [`examples/prettyprint-demo.c`](../examples/prettyprint-demo.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/prettyprint-demo.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
