# MuLIB compiler-scope

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/compiler-scope.c)

`mulib_compiler_scope_t` aliases `mulib_symboltable_t`. `bind` associates a copied,
nonempty name with a caller-provided uint64 symbol; `resolve` returns the nearest
visible binding. Duplicate names in the current scope fail, while inner scopes
may shadow outer names. Failed resolution leaves output unchanged.

Use `mulib_symboltable_enter`, `leave`, and `free` for scope lifecycle. A zeroed
object already has a global scope. Symbols are caller-assigned values, not
automatically generated IDs. Enable SYMBOLTABLE and COMPILER_SCOPE implementations.
See [symboltable](mulib-symboltable.md) for ownership and rollback rules.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-compiler-scope.h`](../include/mulib-compiler-scope.h) |
| Implementation switch | `MULIB_IMPL_COMPILER_SCOPE` |
| Repository-local includes | [mulib-symboltable](mulib-symboltable.md) |
| Complete example | [`examples/modules/compiler-scope.c`](../examples/modules/compiler-scope.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/compiler-scope.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
