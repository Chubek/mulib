# MuLIB typesystem

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/typesystem.c)

`intern` canonicalizes and interns a structural descriptor, returning a stable
index. Primitive VOID/BOOL/I64/F64 descriptors ignore element/count/argument.
POINTER references an existing element and ignores count/argument. ARRAY references
an existing element, retains count (including zero), and ignores argument.
FUNCTION references an existing return type in element; count is arity and, when
nonzero, argument is one existing type shared by all parameters. Zero-arity
functions ignore argument. Heterogeneous parameter lists are not represented.

INVALID/out-of-range kinds or invalid referenced IDs fail without inserting.
`get` returns a borrowed descriptor or NULL; its pointer may move after insertion,
but its ID remains stable. Descriptors refer only to already interned types, so
recursive types are not constructed. Zero-initialize and `free`; lookup/intern
are O(types), storage O(types). No target size/alignment computation is provided.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-typesystem.h`](../include/mulib-typesystem.h) |
| Implementation switch | `MULIB_IMPL_TYPESYSTEM` |
| Repository-local includes | None |
| Complete example | [`examples/modules/typesystem.c`](../examples/modules/typesystem.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/typesystem.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
