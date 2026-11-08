# First-order term parser

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_TREE_PARSING`, `MULIB_IMPL_AST` and `MULIB_IMPL_STRBUF`.
[Example](../examples/modules/tree_parsing.c).

Grammar: `term = identifier [ '(' [ term { ',' term } ] ')' ]`.
Identifiers are `[A-Za-z_][A-Za-z0-9_]*`; variables are `?identifier` leaves
without parentheses. Space, tab, CR and LF are ignored between tokens. Full
input must be consumed; no comments, strings, numbers, operators or infix syntax.
`f` and `f()` have the same tree representation.

Returns an owned `mulib_ast_t` with production=symbol, empty text and byte offset.
Destroy with `mulib_ast_destroy`; existing AST visitors/renderers work unchanged.
NULL input, malformed syntax, depth>128, exhausted caller node_limit or OOM
returns NULL. Optional error_offset is the current byte position, end on success;
syntax/resource/OOM errors are not separately classified. At most node_limit
allocatable nodes, recursive stack depth<=128; input is borrowed, not mutated.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-tree_parsing.h`](../include/mulib-tree_parsing.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-tree_parsing.h
 * @brief Bounded first-order term parsing into the shared MuLIB AST model.
 * Define MULIB_IMPL_TREE_PARSING, MULIB_IMPL_AST and MULIB_IMPL_STRBUF.
 */
#ifndef MULIB_TREE_PARSING_H
#define MULIB_TREE_PARSING_H
#include "mulib-ast.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Parse symbol or symbol(term,...), with ASCII identifiers, whitespace and
 * optional ? prefix for leaf variables. Empty argument lists are accepted.
 * Full input must be consumed. Maximum depth 128; node_limit is a hard cap.
 * Result owns its nodes; release with mulib_ast_destroy. NULL for invalid/OOM/
 * limit; optional error_offset identifies the byte position (end on success).
 * AST production holds the symbol; text is empty. No strings, comments or infix.
 */
mulib_ast_t *mulib_tree_parse(const char *source, size_t node_limit, size_t *error_offset);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-tree_parsing.h`](../include/mulib-tree_parsing.h) |
| Implementation switch | `MULIB_IMPL_TREE_PARSING` |
| Repository-local includes | [mulib-ast](mulib-ast.md) |
| Complete example | [`examples/modules/tree_parsing.c`](../examples/modules/tree_parsing.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/tree_parsing.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
