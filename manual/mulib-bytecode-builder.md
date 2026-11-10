# MuLIB bytecode-builder

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/bytecode-builder.c)

Zero-initialize `mulib_bytecode_builder_t`. `add` appends a copied instruction,
optionally returning its index; opcodes outside PUSH..HALT are rejected. Capacity
grows geometrically, up to 1048576 instructions. `patch` changes the operand of
an existing instruction, leaving its opcode intact. Both return 0 on failure.
Indices survive growth, pointers into `code` do not. Failed operations preserve
the instruction sequence and optional index output.

The builder does not infer branch targets, local counts, or stack correctness.
Run [bytecode-verify](mulib-bytecode-verify.md) before evaluation. `free` releases
the owned code and resets the builder. Append is amortized O(1), patch O(1).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-bytecode-builder.h`](../include/mulib-bytecode-builder.h) |
| Implementation switch | `MULIB_IMPL_BYTECODE_BUILDER` |
| Repository-local includes | [mulib-bytecode](mulib-bytecode.md) |
| Complete example | [`examples/modules/bytecode-builder.c`](../examples/modules/bytecode-builder.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bytecode-builder.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
