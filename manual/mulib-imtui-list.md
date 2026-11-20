# imtui-list — Virtualized list

Submit a visible rectangle and a row provider. The list requests only rows
needed for the viewport. Keep the selection and scroll position in the
caller-owned list state across frames. Keyboard movement is clipped to the
existing rows; a mouse click can focus the list.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-imtui-list.h`](../include/mulib-imtui-list.h) |
| Implementation switch | `MULIB_IMPL_IMTUI_LIST` |
| Repository-local includes | [mulib-imtui-draw](mulib-imtui-draw.md) |
| Complete example | [`examples/imtui/composites.c`](../examples/imtui/composites.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/imtui/composites.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
