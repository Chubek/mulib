# MuLIB frame-layout

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/frame-layout.c)

`add` places a nonzero-sized slot at the next offset aligned to a nonzero power
of two, returning its stable index. Slot size, alignment, and offset are retained.
Overflow and invalid alignment return 0 before insertion. `finish` rounds `used`
up to the requested power-of-two frame alignment without changing the layout.

Offsets are nonnegative relative to the caller's frame base; this module does not
emit a prologue, account for a red zone, or impose ABI alignment. Choose final
alignment to satisfy every slot's base alignment. Zero-initialize and call `free`.
Append is amortized O(1); finish O(1); storage O(slots).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-frame-layout.h`](../include/mulib-frame-layout.h) |
| Implementation switch | `MULIB_IMPL_FRAME_LAYOUT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/frame-layout.c`](../examples/modules/frame-layout.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/frame-layout.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
