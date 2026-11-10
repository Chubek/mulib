# MuLIB stackmap

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/stackmap.c)

`add` appends an exact native-PC safepoint with pointer-register and pointer-spill
bitsets (up to 64 each). PCs must be strictly increasing; duplicates and backward
insertion fail without mutation. PC 0 is valid. Bit meanings and PC units are
caller-defined and must agree with the backend frame/register layout.

`find` requires exact PC equality, not nearest-preceding lookup. It copies the
entry or returns 0 without changing output. Maps describe locations but do not
walk stacks or register roots with GC automatically. Zero-initialize and `free`.
Append is amortized O(1), lookup O(log entries), storage O(entries).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-stackmap.h`](../include/mulib-stackmap.h) |
| Implementation switch | `MULIB_IMPL_STACKMAP` |
| Repository-local includes | None |
| Complete example | [`examples/modules/stackmap.c`](../examples/modules/stackmap.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/stackmap.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
