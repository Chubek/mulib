# imtui-tabs — Terminal tab strip

Submit tab labels and keep the selected tab in caller-owned state.
Keyboard navigation changes the active tab; the application submits only
that tab's contents. The mouse can focus the tab strip.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-imtui-tabs.h`](../kernels/mulib-imtui-tabs.h) |
| Implementation switch | `MULIB_IMPL_IMTUI_TABS` |
| Repository-local includes | [mulib-imtui-draw](mulib-imtui-draw.md) |
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
