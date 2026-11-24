# imtui-draw — Clipped terminal drawing

Fill rectangles, draw borders, and draw UTF-8 text into a frame. Coordinates
are cells and respect the current clip. The text API does not parse terminal
escape sequences. See [ImTUI](mulib-imtui.md) for the width model.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-imtui-draw.h`](../kernels/mulib-imtui-draw.h) |
| Implementation switch | `MULIB_IMPL_IMTUI_DRAW` |
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
