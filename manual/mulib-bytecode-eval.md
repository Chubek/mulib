# MuLIB bytecode-eval

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/bytecode-eval.c)

`mulib_bytecode_eval` verifies a program, allocates exactly the verified peak stack
capacity, and executes it within the supplied instruction budget. It returns 1
only for a successful HALT. Enable BYTECODE_VERIFY and BYTECODE implementations
as well as BYTECODE_EVAL.

The result pointer is required. Verification failure initializes it to INVALID,
zero steps/value, and the verifier's bad instruction index (0 for invalid call
arguments, count when no reachable HALT exists). Execution failure retains the
interpreter's detailed status. Verification does not prove termination or exclude
arithmetic faults; the execution budget remains necessary for bounded work.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-bytecode-eval.h`](../include/mulib-bytecode-eval.h) |
| Implementation switch | `MULIB_IMPL_BYTECODE_EVAL` |
| Repository-local includes | [mulib-bytecode-verify](mulib-bytecode-verify.md) |
| Complete example | [`examples/modules/bytecode-eval.c`](../examples/modules/bytecode-eval.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bytecode-eval.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
