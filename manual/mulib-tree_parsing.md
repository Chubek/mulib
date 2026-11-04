# First-order term parser

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
