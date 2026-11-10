# MuLIB gc-roots

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/gc-roots.c)

`add` stores a non-NULL borrowed object pointer; duplicates succeed without
increasing count. `remove` removes one matching root and returns 0 if absent.
Roots retain insertion order. Registration itself does not verify heap membership.
`collect(heap,roots,...)` delegates to GC, which validates every pointer against
the supplied heap before changing it.

Zero-initialize the root set. Its `free` releases only the pointer array, not
objects. Remove roots before objects become invalid through heap destruction or
collection with a different root set. Enable GC and GC_ROOTS implementations.
Add/remove are O(root count); collection uses the GC contract.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-gc-roots.h`](../include/mulib-gc-roots.h) |
| Implementation switch | `MULIB_IMPL_GC_ROOTS` |
| Repository-local includes | [mulib-gc](mulib-gc.md) |
| Complete example | [`examples/modules/gc-roots.c`](../examples/modules/gc-roots.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/gc-roots.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
