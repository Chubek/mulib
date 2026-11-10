# MuLIB parser-pratt

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/parser-pratt.c)

`parse` builds an owned expression tree from an explicit-length byte slice into
an empty zeroed output. It requires complete consumption (apart from lexer-skipped
comments/whitespace). Failure frees temporary nodes, leaves output unchanged, and
reports the current token's byte offset. Success reports source length.

Operands are decimal integers 0..INT64_MAX or identifiers of at most 63 bytes.
Precedence from low to high: ==, <, +/-, */%, unary +/-; binary operators are
left-associative. Parentheses group expressions. Unary plus produces no node.
Direct INT64_MIN spelling is outside the literal range; it can be computed as
`-9223372036854775807-1`. Node spans identify literal/name/operator tokens,
not whole expressions. Names are copied into nodes; source storage can be released.

Limits are 65536 nodes and 128 parser recursion levels; consuming passes have
their own depth limits, so a very long left-associated tree may parse but fail
recursive evaluation/typechecking. Node IDs survive array growth. `free` resets
the tree. Time O(tokens), storage O(nodes).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-parser-pratt.h`](../include/mulib-parser-pratt.h) |
| Implementation switch | `MULIB_IMPL_PARSER_PRATT` |
| Repository-local includes | [mulib-lexer](mulib-lexer.md) |
| Complete example | [`examples/modules/parser-pratt.c`](../examples/modules/parser-pratt.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/parser-pratt.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
