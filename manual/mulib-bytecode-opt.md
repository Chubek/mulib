# MuLIB bytecode-opt

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/bytecode-opt.c)

`mulib_bytecode_optimize` folds adjacent PUSH, PUSH, arithmetic triples in a
caller-owned instruction array, repeatedly until stable. ADD/SUB/MUL/DIV/EQ use
the interpreter's checked signed-64 semantics. Faulting arithmetic stays intact.
On success the count shrinks and optional `folded` reports the number of folds.

Jumps, invalid opcodes, counts above 1048576, and NULL code/count are rejected
before mutation. Empty non-NULL arrays are accepted. The pass does not validate
stack shape or remove unreachable instructions. Worst-case time O(n²), extra
space O(1). Its implementation can be compiled separately from BYTECODE; checked
arithmetic is supplied by an internal header, not another translation unit.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-bytecode-opt.h`](../include/mulib-bytecode-opt.h) |
| Implementation switch | `MULIB_IMPL_BYTECODE_OPT` |
| Repository-local includes | [`include/detail/mulib-compiler-integer.h`](../include/detail/mulib-compiler-integer.h), [mulib-bytecode](mulib-bytecode.md) |
| Complete example | [`examples/modules/bytecode-opt.c`](../examples/modules/bytecode-opt.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bytecode-opt.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
