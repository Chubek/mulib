# MuLIB bytecode

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/bytecode.c)

`mulib_bytecode_run` executes immutable signed-64 stack instructions. PUSH uses
its literal operand; LOAD/STORE index zero-initialized locals; ADD/SUB/MUL/DIV/EQ
consume two values and push one. Arithmetic overflow and division by zero fail.
JMP targets an absolute instruction index; JZ pops a condition and branches when
zero. Both validate their target, including an untaken JZ. HALT requires a value
and returns the top of the stack; falling off the program is invalid.

Code count is 1..1048576; local count and stack limit are at most 1048576.
The step budget counts executed instructions, including HALT and faulting
instructions. Zero budget immediately returns LIMIT. A non-NULL result is filled
on every call with status, current instruction, steps, and value (only meaningful
on OK). Status distinguishes invalid code, stack bounds, arithmetic, budget, and
allocation failures. Execution checks visited instructions; use
[bytecode-verify](mulib-bytecode-verify.md) to validate the full instruction array.
Memory is O(locals + stack limit), time O(executed steps).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-bytecode.h`](../include/mulib-bytecode.h) |
| Implementation switch | `MULIB_IMPL_BYTECODE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/bytecode.c`](../examples/modules/bytecode.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bytecode.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
