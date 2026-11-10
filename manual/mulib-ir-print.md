# MuLIB ir-print

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/ir-print.c)

`mulib_ir_print` prints one line per compact IR instruction as
`index: mnemonic rDEST, rA, rB, immediate\n`. Unused fields are printed too.
It validates opcode range but does not check definition ordering; use
[ir-verify](mulib-ir-verify.md) for that.

Return value is bytes written excluding NUL, or 0 on invalid input/short buffer.
Output and positive capacity are required; failures may leave a NUL-terminated
prefix. Empty code produces an empty string and also returns 0. There is no size
query mode. Time O(instructions + text length), fixed temporary line storage.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ir-print.h`](../include/mulib-ir-print.h) |
| Implementation switch | `MULIB_IMPL_IR_PRINT` |
| Repository-local includes | [mulib-ir-verify](mulib-ir-verify.md) |
| Complete example | [`examples/modules/ir-print.c`](../examples/modules/ir-print.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ir-print.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
