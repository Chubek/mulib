# MuLIB consteval

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/consteval.c)

`mulib_consteval_eval` evaluates a Pratt tree using signed 64-bit literals and
symbol values. Scope values are interpreted as two's-complement bit patterns.
Supported operators are unary minus, +, -, *, /, %, ==, and <; comparisons produce
0 or 1. Unary plus is removed by the parser. Division truncates toward zero.

Overflow, zero divisors, INT64_MIN / -1 (also remainder), unresolved names,
invalid nodes, and recursion beyond depth 128 return 0 without changing output.
Scope can be NULL for expressions without names. Trees and scopes are borrowed.
The recursive evaluator is intended for parser-produced trees; depth bounds also
stop cycles. Time is O(visited nodes plus symbol lookup), stack O(depth).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-consteval.h`](../include/mulib-consteval.h) |
| Implementation switch | `MULIB_IMPL_CONSTEVAL` |
| Repository-local includes | [mulib-parser-pratt](mulib-parser-pratt.md), [mulib-symboltable](mulib-symboltable.md) |
| Complete example | [`examples/modules/consteval.c`](../examples/modules/consteval.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/consteval.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
