# `mulib-markup-stream`

Scan caller-owned bytes into source tokens, parse an XML-like fragment into an
owned tree, or preserve the source and tokens in a concrete syntax tree (CST).
Tree events provide a borrowed, mutation-sensitive traversal of one subtree.
This is a deliberately small grammar, not a standards-compliant XML or HTML
processor.

`mulib_cdd_mkir_cst_token_bytes` retrieves an exact source slice from the
snapshot; editing the tree does not alter the snapshot. Use
`mulib_cdd_mkir_events_walk_bounded` to limit callback depth and count.
CDATA sections produce `MULIB_MKIR_T_CDATA` tokens and text events carrying
literal CDATA values.

See [Markup IR](markup-ir.md) for grammar, bounds, source offsets, errors and
implementation switches.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-markup-stream.h`](../kernels/mulib-markup-stream.h) |
| Implementation switch | `MULIB_IMPL_CDD_MARKUP_STREAM` |
| Repository-local includes | [mulib-markup-tree](mulib-markup-tree.md) |
| Complete example | [`examples/markup-ir.c`](../examples/markup-ir.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/markup-ir.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
