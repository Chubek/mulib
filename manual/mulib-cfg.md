# MuLIB cfg

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/cfg.c)

Zero-initialize a graph and set `blocks` before adding edges. `add` copies an
in-range directed edge; duplicate insertion succeeds without adding another edge.
`has` tests membership. `validate` checks edge storage/counts and endpoints.
Block IDs are stable indices; leave container bookkeeping to the API.

`reachable` returns a 64-bit mask for 1..64 blocks and a valid entry. `postorder`
uses iterative DFS, following edges in insertion order, and emits reachable blocks
only. Output capacity need only cover the reachable count. Both preserve output
on failure, including invalid edges or insufficient capacity. `free` resets the
graph. Add/has are O(E); reachability is O(VE), postorder O(VE), with O(V) scratch.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-cfg.h`](../include/mulib-cfg.h) |
| Implementation switch | `MULIB_IMPL_CFG` |
| Repository-local includes | None |
| Complete example | [`examples/modules/cfg.c`](../examples/modules/cfg.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/cfg.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
