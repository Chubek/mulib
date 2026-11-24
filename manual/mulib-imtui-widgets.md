# imtui-widgets — Immediate terminal controls

Submit buttons, labels, checkboxes, sliders and other controls each frame.
Keep the model in caller-owned variables and use stable IDs so focus and
press state persist across frames. Check return values for activation or
changes; disabled controls render without accepting interaction.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-imtui-widgets.h`](../kernels/mulib-imtui-widgets.h) |
| Implementation switch | `MULIB_IMPL_IMTUI_WIDGETS` |
| Repository-local includes | [mulib-imtui-draw](mulib-imtui-draw.md) |
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
