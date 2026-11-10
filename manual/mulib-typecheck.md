# MuLIB typecheck

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/typecheck.c)

`expr` checks a Pratt expression against interned types and an optional symbol
scope mapping names to type IDs. Literals need an interned I64 type. Arithmetic
and unary negation require I64; == and < compare I64 values and return an interned
BOOL. Unary plus is eliminated by the parser. Both operands must share a type ID.

Missing symbols/types, invalid IDs/operators, incompatible operands, and recursion
beyond depth 128 fail without changing output. The type table is not extended
implicitly. This subset does not provide coercions, F64 arithmetic, Boolean
equality, pointer arithmetic, or calls, even though typesystem can describe other
types. Trees/scopes/types are borrowed. Work is recursive over visited nodes.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-typecheck.h`](../include/mulib-typecheck.h) |
| Implementation switch | `MULIB_IMPL_TYPECHECK` |
| Repository-local includes | [mulib-parser-pratt](mulib-parser-pratt.md), [mulib-symboltable](mulib-symboltable.md), [mulib-typesystem](mulib-typesystem.md) |
| Complete example | [`examples/modules/typecheck.c`](../examples/modules/typecheck.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/typecheck.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
