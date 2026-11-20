# imtui-render — Incremental terminal output

Render a completed [ImTUI frame](mulib-imtui-core.md) against explicit
Terminal capabilities. The renderer retains the previously presented cells
and encodes changed runs. Keep emitted bytes in order and write all of them
before discarding the output. Invalidate the comparison baseline if it no
longer matches the terminal (for example, after printing outside the UI).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-imtui-render.h`](../include/mulib-imtui-render.h) |
| Implementation switch | `MULIB_IMPL_IMTUI_RENDER` |
| Repository-local includes | [mulib-imtui-core](mulib-imtui-core.md), [mulib-termstyle](mulib-termstyle.md) |
| Complete example | [`examples/imtui/terminal.c`](../examples/imtui/terminal.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/imtui/terminal.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
