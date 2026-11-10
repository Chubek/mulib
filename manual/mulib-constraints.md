# MuLIB constraints

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/constraints.c)

`new` appends an unbound variable and returns its stable index. `unify` merges
equivalence classes using union by rank and path compression. `bind` assigns a
concrete size_t type to a class; SIZE_MAX is reserved for unbound. Existing
compatible assignments succeed, conflicting bindings/unifications return 0
without changing the logical assignment (path compression may still occur).

`type` returns the concrete class type, or 0 for unbound/invalid input without
changing its output. Variable IDs must come from this container. Zero-initialize
and `free` after use. Amortized union/find is inverse-Ackermann time; storage O(n).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-constraints.h`](../include/mulib-constraints.h) |
| Implementation switch | `MULIB_IMPL_CONSTRAINTS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/constraints.c`](../examples/modules/constraints.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/constraints.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
