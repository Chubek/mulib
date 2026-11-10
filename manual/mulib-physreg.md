# MuLIB physreg

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/physreg.c)

`mulib_physreg_t` describes up to 64 registers with masks. `validate` requires
available and reserved to be disjoint, caller_saved and callee_saved to be
disjoint, and both save classes to be subsets of available. Available registers
need not all belong to a save class.

`pick` selects the lowest available bit not present in `occupied`, writing its
index on success. Invalid descriptors, NULL output, or exhaustion return 0
without modifying output. The descriptor and occupied mask are not modified.
There are no architectural defaults or allocations; lookup examines at most 64 bits.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-physreg.h`](../include/mulib-physreg.h) |
| Implementation switch | `MULIB_IMPL_PHYSREG` |
| Repository-local includes | None |
| Complete example | [`examples/modules/physreg.c`](../examples/modules/physreg.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/physreg.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
