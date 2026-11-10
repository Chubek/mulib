# MuLIB callconv

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/callconv.c)

`mulib_callconv_arguments` places only eight-byte integer/pointer arguments.
AMD64 SysV uses architectural register indices 7,6,2,1,8,9 (RDI, RSI, RDX, RCX,
R8, R9); RV64 LP64 uses 10..17 (a0..a7). Remaining arguments have `reg == -1`
and successive stack offsets 0,8,16,... relative to the outgoing argument area.
Register arguments have `stack_offset == SIZE_MAX`.

`stack_size` is the raw argument area size, not rounded call-frame alignment and
not inclusive of a return address. The caller supplies frame alignment/prologue.
Count is at most 1048576; NULL output is allowed for zero arguments. Invalid ABI
or bounds return 0 before output changes. Floating-point, aggregates, variadics,
and return-value placement are outside this API. Time O(n), scratch O(1).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-callconv.h`](../include/mulib-callconv.h) |
| Implementation switch | `MULIB_IMPL_CALLCONV` |
| Repository-local includes | None |
| Complete example | [`examples/modules/callconv.c`](../examples/modules/callconv.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/callconv.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
