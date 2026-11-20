# termscript-compiler — Canonical AST to C11

The compiler validates the tree and structured control flow using explicit
traversal and control stacks. It packs the AST into a canonical representation
and folds safe constant expressions. The generated C11 contains an executable
AST and a callable entry function. It links to the Termscript runtime without
the compiler or source parser; the runtime walks that AST rather than inlining
native C operations and branches.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-termscript-compiler.h`](../include/mulib-termscript-compiler.h) |
| Implementation switch | `MULIB_IMPL_TERMSCRIPT_COMPILER` |
| Repository-local includes | [mulib-strbuf](mulib-strbuf.md), [mulib-termscript-ast](mulib-termscript-ast.md) |
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
