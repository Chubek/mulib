# termscript-ast — Parsec-backed syntax tree

The parser owns a copy of source and constructs an indexed tree with byte
spans, first-child and next-sibling links. `SIZE_MAX` denotes a missing link.
It parses without running code or touching the terminal. Start with zeroed
storage and free the program once no VM values or definitions borrow it.
The parser distinguishes escape values such as `[[CUP]]` from
comma-separated key chords such as `[[ctrl, f1]]`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-termscript-ast.h`](../include/mulib-termscript-ast.h) |
| Implementation switch | `MULIB_IMPL_TERMSCRIPT_AST` |
| Repository-local includes | [mulib-parsec](mulib-parsec.md), [mulib-utf8](mulib-utf8.md) |
| Complete example | [`library/termscript/main.c`](../library/termscript/main.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude library/termscript/main.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
