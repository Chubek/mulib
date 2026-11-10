# MuLIB module-resolver

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/module-resolver.c)

`add` copies a nonempty module name and a NUL-terminated source path. Duplicate
names are rejected; paths may be empty. `get` performs exact case-sensitive name
lookup and returns a borrowed path, or NULL on missing/invalid input. No file
access, path normalization, search paths, or import loading occurs.

Zero-initialize the resolver. Failed additions preserve existing entries. Returned
path bytes survive further insertions and remain valid until `free`; pointers
into the entries array may move on growth. `free` releases both names and paths.
Lookup is O(entries × compared name bytes), insertion adds copying costs.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-module-resolver.h`](../include/mulib-module-resolver.h) |
| Implementation switch | `MULIB_IMPL_MODULE_RESOLVER` |
| Repository-local includes | None |
| Complete example | [`examples/modules/module-resolver.c`](../examples/modules/module-resolver.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/module-resolver.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
