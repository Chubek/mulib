# `mulib-markup-tree`

An owned, ordered arena of markup nodes with stable numeric IDs. Nodes own
names, arbitrary byte values and optional source coordinates; the tree owns
element attributes. Initialize a tree to zero and release it with
`mulib_mkir_tree_free`. Mutation can invalidate borrowed node and attribute
pointers without changing node IDs.

`mulib_mkir_tree_attach` appends or detaches an existing node.
`mulib_cdd_mkir_tree_insert_before` also supports moving a node immediately
before a sibling; moving a node under its descendant is rejected. Validation
checks the entire forest, not only nodes reachable from the first root.

See [Markup IR](markup-ir.md) for topology, ownership and SXML serialization
contracts and the [stream kernel](mulib-markup-stream.md) for parsing.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-markup-tree.h`](../include/mulib-markup-tree.h) |
| Implementation switch | `MULIB_IMPL_CDD_MARKUP_TREE` |
| Repository-local includes | None |
| Complete example | [`examples/markup-ir.c`](../examples/markup-ir.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/markup-ir.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
