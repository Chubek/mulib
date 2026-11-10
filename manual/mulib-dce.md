# MuLIB dce

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/dce.c)

`mulib_dce_mark` walks a linear instruction sequence backward. An instruction is
kept if effectful or if its destination is live; retained instructions kill their
destination and make their sources live. `live_out` supplies exit roots and
`marks[i]` is 1 for retained instructions. The input sequence is not compacted.

Registers are 0..63, SIZE_MAX means no destination, and each instruction has at
most two uses. Validation precedes output writes. Empty NULL arrays are accepted;
invalid registers/counts fail with marks unchanged. Mark potentially trapping
instructions effectful when their faults must be preserved. This is linear DCE,
not a CFG fixed-point pass. Time O(n), extra storage O(1).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dce.h`](../include/mulib-dce.h) |
| Implementation switch | `MULIB_IMPL_DCE` |
| Repository-local includes | [mulib-defuse](mulib-defuse.md) |
| Complete example | [`examples/modules/dce.c`](../examples/modules/dce.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dce.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
