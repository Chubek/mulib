# MuLIB gc

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/gc.c)

Zero-initialize a heap. `alloc` creates an object with the requested number of
NULL reference slots and returns NULL on failure. Objects contain reference
slots, not arbitrary payload storage. `set` checks parent/child heap membership
and slot bounds; NULL clears a reference. Object addresses remain stable until
collected or heap destruction. Use the API rather than editing object links.

`collect` validates roots and child references, marks iteratively, then sweeps
unreachable objects, including cycles. NULL root entries are ignored; duplicate
roots are allowed. `reclaimed` is required and written only on success. Invalid
references/allocation failure cause no collection. Unregistered raw pointers do
not keep objects alive. `heap_free` frees everything. Collection has O(H(R+S))
membership-validation cost plus O(H+S+R) tracing, with O(H) scratch, for H objects,
R roots, S slots. Collection is explicit and stop-the-world, with no finalizers.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-gc.h`](../include/mulib-gc.h) |
| Implementation switch | `MULIB_IMPL_GC` |
| Repository-local includes | None |
| Complete example | [`examples/modules/gc.c`](../examples/modules/gc.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/gc.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
