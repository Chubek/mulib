# MuLIB bytecode-disasm

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/bytecode-disasm.c)

`mulib_bytecode_disassemble` prints `index: OPCODE operand\n` for each instruction,
including unused operands. It returns bytes written excluding the trailing NUL.
The output pointer and a positive capacity are required. An empty instruction
array produces an empty string and returns 0; NULL code is allowed only at count 0.

Invalid opcodes or insufficient capacity return 0 and may leave a terminated
prefix. This is not a required-size query or a control-flow verifier. Time is
O(instructions + output bytes), with fixed-size temporary line storage. Only this
module's implementation switch is needed; bytecode supplies declaration types.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-bytecode-disasm.h`](../include/mulib-bytecode-disasm.h) |
| Implementation switch | `MULIB_IMPL_BYTECODE_DISASM` |
| Repository-local includes | [mulib-bytecode](mulib-bytecode.md) |
| Complete example | [`examples/modules/bytecode-disasm.c`](../examples/modules/bytecode-disasm.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bytecode-disasm.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
