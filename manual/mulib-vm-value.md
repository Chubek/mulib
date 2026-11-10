# MuLIB vm-value

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/vm-value.c)

Zero-initialize values (NIL). Scalars hold BOOL, I64, or F64. BYTES owns an explicit
length allocation with no implicit terminator; embedded NULs are ordinary bytes.
`bytes` copies input before releasing the previous value, so replacing from a
slice of the current bytes is supported. NULL data is accepted at length zero.

`copy` deep-copies BYTES, copies scalar payloads, and accepts self-copy for valid
kinds. Invalid kinds/input/allocation failure leave destination unchanged. Both
source and destination must be initialized valid values; never shallow-copy owning
BYTES values. `free` releases bytes and resets to NIL. Byte operations cost O(length),
scalar operations O(1); no numeric coercion or string parsing is performed.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-vm-value.h`](../include/mulib-vm-value.h) |
| Implementation switch | `MULIB_IMPL_VM_VALUE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/vm-value.c`](../examples/modules/vm-value.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/vm-value.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
