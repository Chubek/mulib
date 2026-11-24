# `mulib-prettyemit.h`: annotated source emitters

`mulib_pretty_emit` turns a layout from `mulib_pretty_layout` into plain text,
HTML with token-level MathML, LaTeX, or troff. Markup is escaped after layout;
it does not affect line-breaking decisions. Pass a successfully built layout,
not arbitrary pieces and offsets.

`mulib_pretty_render` builds and disposes of the intermediate layout for a
document/root pair. Both functions return a malloc-owned C string to release
with `free`, or `NULL` on failure. See [PrettyDoc](mulib-prettydoc.md#render-targets)
for supported markup and limits.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-prettyemit.h`](../kernels/mulib-prettyemit.h) |
| Implementation switch | `MULIB_IMPL_PRETTYEMIT` |
| Repository-local includes | [mulib-prettydoc](mulib-prettydoc.md) |
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
