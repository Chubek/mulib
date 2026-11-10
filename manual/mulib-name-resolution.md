# MuLIB name-resolution

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/name-resolution.c)

`run` examines every Pratt node, mapping NAME nodes through a lexical symbol
table. Output is indexed by node ID; non-name nodes receive SIZE_MAX. Symbol
values must fit size_t and must not equal SIZE_MAX, which is reserved as sentinel.
Capacity must cover every node, not only identifier occurrences.

Unresolved names or unrepresentable IDs return 0, set optional `failure_node`,
and preserve the entire destination. Success sets failure_node to SIZE_MAX.
Argument/allocation failure leaves it unchanged. A NULL scope is allowed if no
names need lookup. This does not assign IDs or resolve declarations; use the
symbol table to populate bindings first. Scratch O(nodes), time includes lookup.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-name-resolution.h`](../include/mulib-name-resolution.h) |
| Implementation switch | `MULIB_IMPL_NAME_RESOLUTION` |
| Repository-local includes | [mulib-parser-pratt](mulib-parser-pratt.md), [mulib-symboltable](mulib-symboltable.md) |
| Complete example | [`examples/modules/name-resolution.c`](../examples/modules/name-resolution.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/name-resolution.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
