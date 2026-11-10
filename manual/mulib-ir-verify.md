# MuLIB ir-verify

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/ir-verify.c)

`verify` checks a linear single-assignment register IR: LITERAL/PARAM define a
register, COPY uses one, arithmetic/EQ use two, and RETURN uses one without a
destination. Every used register must already be defined; redefinition is invalid.
Exactly one RETURN must terminate the sequence. Register count is at most 1048576.

An optional bad index receives the first failing instruction, SIZE_MAX on success,
or count if RETURN is missing. Invalid arguments/allocation failure leave it
unchanged. Immediate/PARAM metadata is application-defined. The verifier does not
model blocks, phi nodes, types, arithmetic values, or a parameter table. It uses
O(instructions + registers) time and O(registers) temporary storage.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ir-verify.h`](../include/mulib-ir-verify.h) |
| Implementation switch | `MULIB_IMPL_IR_VERIFY` |
| Repository-local includes | None |
| Complete example | [`examples/modules/ir-verify.c`](../examples/modules/ir-verify.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ir-verify.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
