# imtui-edit — Single-line UTF-8 editor

The editor maintains a byte selection at UTF-8 scalar boundaries over the
caller-owned text buffer. Navigate with the keyboard and insert decoded
characters or bracketed-paste data. Capacity is fixed by the caller.
Text entry does not interpret input as terminal escape sequences.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-imtui-edit.h`](../kernels/mulib-imtui-edit.h) |
| Implementation switch | `MULIB_IMPL_IMTUI_EDIT` |
| Repository-local includes | [mulib-imtui-draw](mulib-imtui-draw.md), [mulib-utf8](mulib-utf8.md) |
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
