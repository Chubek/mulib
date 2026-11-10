# MuLIB bytecode-verify

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/bytecode-verify.c)

`mulib_bytecode_verify` checks all opcodes, local indices, and absolute jump
targets, including unreachable instructions. A worklist propagates stack heights
from instruction 0: underflow, mismatched branch joins, and reachable fallthrough
past the array are rejected. At least one reachable HALT with a value is required.
Unreachable instructions do not contribute to stack height analysis.

Counts are 1..1048576 and locals at most 1048576. Required `max_stack` is written
only on success; optional `bad_pc` is SIZE_MAX on success, the fault index on
instruction failure, count for no reachable HALT, or 0 for argument/allocation
failure. Time and scratch are O(n). Verification does not prove termination,
check arithmetic values, or require exactly one stack value at HALT.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-bytecode-verify.h`](../include/mulib-bytecode-verify.h) |
| Implementation switch | `MULIB_IMPL_BYTECODE_VERIFY` |
| Repository-local includes | [mulib-bytecode](mulib-bytecode.md) |
| Complete example | [`examples/modules/bytecode-verify.c`](../examples/modules/bytecode-verify.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bytecode-verify.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
