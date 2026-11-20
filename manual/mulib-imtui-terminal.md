# imtui-terminal — Terminal session I/O

The terminal backend adapts POSIX tty state, byte input and nonblocking
output to the ImTUI frame loop. It uses [termkey](mulib-termkey.md) to decode
fragmented input. Standalone Escape is ambiguous; use the application's
explicit Escape timeout. Keep pending output until it has been written.

The application owns signal handling and orderly terminal restoration.
Always try to leave terminal mode before destroying its descriptors.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-imtui-terminal.h`](../include/mulib-imtui-terminal.h) |
| Implementation switch | `MULIB_IMPL_IMTUI_TERMINAL` |
| Repository-local includes | [mulib-terminfo](mulib-terminfo.md), [mulib-termkey](mulib-termkey.md), [mulib-termutils](mulib-termutils.md) |
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
