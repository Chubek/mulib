# imtui-mune — Mune layout and widgets

Register native bindings with a [Mune](mulib-mune.md) runtime to build
terminal layouts and controls from Lua scripts. Scripts use the current
immediate-mode context, and must draw only while that context has an active
frame. Keep the bridge and GUI alive while the registered callbacks can run.

See the standard modules under [`library/imtui`](../library/imtui/) for
layout and widget compositions.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-imtui-mune.h`](../kernels/mulib-imtui-mune.h) |
| Implementation switch | `MULIB_IMPL_IMTUI_MUNE` |
| Repository-local includes | [mulib-imtui-layout](mulib-imtui-layout.md), [mulib-imtui-syntax](mulib-imtui-syntax.md), [mulib-imtui-widgets](mulib-imtui-widgets.md), [mulib-mune](mulib-mune.md) |
| Complete example | [`tests/test_imtui_mune.c`](../tests/test_imtui_mune.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels tests/test_imtui_mune.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
