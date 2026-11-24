# imtui-core — Frames, focus, and terminal cells

Initialize a context with terminal dimensions, and begin each frame with a
batch of decoded [termkey](mulib-termkey.md) events. Use stable, scoped IDs
for interactive rectangles. Focus and input handling are local to the
context. Draw into the context only during a frame, and present the finished
frame after ending it.

Coordinates are zero-based cells; rectangles exclude their right and bottom
edges. `mulib_imtui_cells` exposes a borrowed row-major buffer. Do not retain
the view across a resize or free.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-imtui-core.h`](../kernels/mulib-imtui-core.h) |
| Implementation switch | `MULIB_IMPL_IMTUI_CORE` |
| Repository-local includes | [mulib-termkey](mulib-termkey.md), [mulib-vterm](mulib-vterm.md) |
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
