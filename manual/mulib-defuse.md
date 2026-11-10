# MuLIB defuse

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/defuse.c)

`mulib_defuse_analyze` counts source occurrences and records the first definition
index for each register. SIZE_MAX is the no-destination sentinel; never-defined
registers receive SIZE_MAX. Repeated definitions and use-before-definition are
permitted: this analysis counts occurrences, it does not verify SSA.

Each instruction has up to two source operands; all used register indices must
be below `variables`. `effectful` is irrelevant to counting. Output requires one
record per register and remains unchanged on failure. NULL arrays are accepted
only at corresponding zero counts. Time O(instructions + registers), scratch
O(registers). The caller retains ownership of input and output arrays.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-defuse.h`](../include/mulib-defuse.h) |
| Implementation switch | `MULIB_IMPL_DEFUSE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/defuse.c`](../examples/modules/defuse.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/defuse.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
