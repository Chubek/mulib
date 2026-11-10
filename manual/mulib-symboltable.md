# MuLIB symboltable

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/symboltable.c)

A zeroed table has a global scope. `enter` saves the current binding count;
`leave` removes and frees all bindings added since the latest enter. Leaving
global scope returns 0. `define` copies a nonempty NUL-terminated name with an
arbitrary uint64 value. Duplicate names in the current scope fail; shadowing
outer scopes is allowed. Values are caller-assigned, not generated symbol IDs.

`lookup` scans newest to oldest and writes the first visible value, leaving output
unchanged on a miss. Bindings disappear on scope rollback; indices into the
internal array are not persistent IDs. `free` releases names and scope storage.
Define/lookup are O(bindings × compared bytes); enter is amortized O(1), leave
O(bindings removed). There is no mutation of an existing binding.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-symboltable.h`](../include/mulib-symboltable.h) |
| Implementation switch | `MULIB_IMPL_SYMBOLTABLE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/symboltable.c`](../examples/modules/symboltable.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/symboltable.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
