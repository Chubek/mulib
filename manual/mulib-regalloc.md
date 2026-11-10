# MuLIB regalloc

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/regalloc.c)

`allocate` dispatches LINEAR to [regalloc-linear](mulib-regalloc-linear.md) and
GRAPH to [regalloc-graph](mulib-regalloc-graph.md). LINEAR uses input intervals
and ignores interference; GRAPH uses interference and ignores intervals. Locations
are indexed by input position, never by an interval's application value ID.

A register location has its physical bit index and spill == SIZE_MAX. A spilled
location has reg == -1 and a unique zero-based slot. Graph-mode spill slots are
assigned in node order. Invalid algorithms fail before modifying output. Empty
inputs accept NULL arrays and yield zero spills. Both allocator implementations
must be linked for this dispatcher, even when using only one mode.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-regalloc.h`](../include/mulib-regalloc.h) |
| Implementation switch | `MULIB_IMPL_REGALLOC` |
| Repository-local includes | [mulib-regalloc-graph](mulib-regalloc-graph.md), [mulib-regalloc-linear](mulib-regalloc-linear.md) |
| Complete example | [`examples/modules/regalloc.c`](../examples/modules/regalloc.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/regalloc.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
