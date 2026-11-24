# imtui-scroll — Scrollable viewports

Maintain a caller-owned scroll offset and derive the visible viewport from
content and display dimensions. Use scoped clipping around the contents of
a viewport and balance clip begin and end calls. Both rendering and hit
testing respect the resulting clip.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-imtui-scroll.h`](../kernels/mulib-imtui-scroll.h) |
| Implementation switch | `MULIB_IMPL_IMTUI_SCROLL` |
| Repository-local includes | [mulib-imtui-core](mulib-imtui-core.md) |
| Complete example | [`examples/imtui/composites.c`](../examples/imtui/composites.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/imtui/composites.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
