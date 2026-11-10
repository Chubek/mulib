# MuLIB postdominance

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/postdominance.c)

`compute` reverses CFG edges and computes dominance rooted at the selected exit.
The CFG must have 1..64 blocks and the exit must be in range with no outgoing
edges. Output uses `mulib_dominance_t`; `test(result,a,b)` asks whether a
postdominates b in the exit-reaching subgraph.

Blocks that cannot reach this exit have empty sets and SIZE_MAX immediate parents.
Other exits and nonterminating paths are excluded: this is exit-relative
postdominance, not a guarantee that all executions terminate at the selected exit.
Invalid input/allocation failure leaves output unchanged. Temporary reversed
edges use O(E) storage; analysis costs follow [dominance](mulib-dominance.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-postdominance.h`](../include/mulib-postdominance.h) |
| Implementation switch | `MULIB_IMPL_POSTDOMINANCE` |
| Repository-local includes | [mulib-dominance](mulib-dominance.md) |
| Complete example | [`examples/modules/postdominance.c`](../examples/modules/postdominance.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/postdominance.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
