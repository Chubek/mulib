# MuLIB loops

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/loops.c)

`find` requires a CFG of at most 64 blocks and dominance computed for that exact
unchanged graph. An edge tail→header is a backedge when header dominates tail.
For each backedge, reverse predecessor closure forms the natural-loop membership
mask, excluding unreachable predecessors and stopping at the header.

Results retain edge insertion order, with separate entries for separate backedges
even if headers coincide. A self-loop contains only its header. At most 64 loop
entries fit; overflow/invalid graphs return 0 without changing the result. This
does not identify irreducible cycles or merge nested loops. Storage is fixed;
worst-case work is O(backedges × V × E).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-loops.h`](../include/mulib-loops.h) |
| Implementation switch | `MULIB_IMPL_LOOPS` |
| Repository-local includes | [mulib-dominance](mulib-dominance.md) |
| Complete example | [`examples/modules/loops.c`](../examples/modules/loops.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/loops.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
