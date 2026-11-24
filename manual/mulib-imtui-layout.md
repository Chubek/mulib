# imtui-layout — Integer-cell layouts

Use row, column, grid and weighted track helpers to divide a rectangle.
Equal tracks give remainder cells to the earlier tracks. Padding, alignment,
and splits allow nested layouts. Operations are allocation-free and compute
their outputs without accessing the terminal.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-imtui-layout.h`](../kernels/mulib-imtui-layout.h) |
| Implementation switch | `MULIB_IMPL_IMTUI_LAYOUT` |
| Repository-local includes | [mulib-imtui-core](mulib-imtui-core.md) |
| Complete example | [`examples/imtui/headless.c`](../examples/imtui/headless.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/imtui/headless.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
