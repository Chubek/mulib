# MuLIB dominance

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/dominance.c)

`compute` derives dominance sets and immediate dominators for a valid CFG with
1..64 blocks and a selected entry. `dom[b]` contains every block dominating b,
including b itself. `reachable` marks blocks reachable from entry; unreachable
blocks have empty sets. Entry and unreachable `idom` entries are SIZE_MAX.

`test(d,a,b)` asks whether a dominates b. The result is a value object with no
owned allocations; failed computation leaves it unchanged. It must be recomputed
after graph changes before use in loop analysis. The fixed-point algorithm is
bounded by graph size, with worst-case O(V²E + V²) work and fixed 64-block storage.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dominance.h`](../include/mulib-dominance.h) |
| Implementation switch | `MULIB_IMPL_DOMINANCE` |
| Repository-local includes | [mulib-cfg](mulib-cfg.md) |
| Complete example | [`examples/modules/dominance.c`](../examples/modules/dominance.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dominance.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
