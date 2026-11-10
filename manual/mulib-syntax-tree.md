# MuLIB syntax-tree

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/syntax-tree.c)

`add` appends a node with application kind and half-open byte span. SIZE_MAX
parent creates a root; otherwise the parent must already exist and contain the
child's entire span. Roots may be multiple and zero-length spans are allowed.
IDs are append indices; child/sibling links follow insertion order, with SIZE_MAX
sentinels. Array addresses may change on growth.

`validate` checks span overflow/containment, parent-before-child ordering, complete
child membership, first/last links, increasing sibling indices, and root sibling
rules. Broken links/cycles or orphaned children return 0. This represents a
forest and permits overlapping sibling spans; it is not a grammar validator.
Use links for traversal; there is no callback traversal function. Zero-initialize
and `free`. Add is amortized O(1), valid-tree validation O(nodes).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-syntax-tree.h`](../include/mulib-syntax-tree.h) |
| Implementation switch | `MULIB_IMPL_SYNTAX_TREE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/syntax-tree.c`](../examples/modules/syntax-tree.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/syntax-tree.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
