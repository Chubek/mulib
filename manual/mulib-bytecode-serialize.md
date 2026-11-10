# MuLIB bytecode-serialize

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/bytecode-serialize.c)

`encode` allocates a byte buffer; `decode` allocates an instruction array. Initialize
the output pointer to NULL and release successful output with `free`. Failures
leave output pointer and count/length unchanged. Counts are limited to 1048576;
zero instructions round-trip even though they are not an executable program.

Wire format: ASCII `MBC1`, little-endian u32 count, count records of u8 opcode plus
little-endian two's-complement u64 operand, then little-endian u32 FNV-1a of all
preceding bytes (offset basis 2166136261, multiplier 16777619). Length is 12+9n.
Decoding rejects truncation, trailing bytes, invalid opcodes, and checksum mismatch.
The checksum detects accidental corruption, not authenticity. Stack/control-flow
verification is separate. Time and allocated memory are O(n).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-bytecode-serialize.h`](../include/mulib-bytecode-serialize.h) |
| Implementation switch | `MULIB_IMPL_BYTECODE_SERIALIZE` |
| Repository-local includes | [mulib-bytecode](mulib-bytecode.md) |
| Complete example | [`examples/modules/bytecode-serialize.c`](../examples/modules/bytecode-serialize.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bytecode-serialize.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
