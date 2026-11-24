# imtui — Immediate-mode terminal UI

ImTUI builds a cell frame from the widgets your application submits each
iteration. Application values live outside the frame; the context tracks
focus, activation, scopes, and clipping using stable IDs. It does not use
curses or require an external terminal library.

1. Initialize a context for the current number of terminal columns and rows.
2. Decode input using `mulib-termkey` directly, or use the POSIX terminal
   backend. Begin a frame with the resulting events.
3. Allocate rectangles with the layout helpers and submit visible widgets.
   Supply stable IDs, including inside repeated groups. Draw only during a
   frame and balance all clip and ID scopes.
4. End the frame. Generate terminal bytes into a buffer with the differential
   renderer or generate plain text with `mulib_imtui_snapshot`. Keep queued
   output in wire order; a partial write does not authorize discarding bytes.
5. Close the terminal session and restore tty state on every exit path.

The model uses one Unicode scalar per cell, not general grapheme shaping.
Keep columns and borders in a supported single-width character set where
exact wrapping matters. Text widgets never treat application strings as
terminal control sequences. Capability and tty setup are explicit.

The [Mune bridge](mulib-imtui-mune.md) lets the same immediate context run
layout and widgets from [the ImTUI standard library](../library/imtui/).
For headless checks, compare snapshots and feed decoded key events directly.

Run `make check-imtui` for headless tests. For a live demo on POSIX run
`make build/imtui-terminal` and then `./build/imtui-terminal` from a foreground
terminal. Press Tab/Space, click with the mouse, or press Q to quit.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-imtui.h`](../kernels/mulib-imtui.h) |
| Implementation switch | `MULIB_IMPL_IMTUI` |
| Repository-local includes | [mulib-imtui-core](mulib-imtui-core.md), [mulib-imtui-draw](mulib-imtui-draw.md), [mulib-imtui-edit](mulib-imtui-edit.md), [mulib-imtui-layout](mulib-imtui-layout.md), [mulib-imtui-list](mulib-imtui-list.md), [mulib-imtui-mune](mulib-imtui-mune.md), [mulib-imtui-render](mulib-imtui-render.md), [mulib-imtui-scroll](mulib-imtui-scroll.md), [mulib-imtui-syntax](mulib-imtui-syntax.md), [mulib-imtui-table](mulib-imtui-table.md), [mulib-imtui-tabs](mulib-imtui-tabs.md), [mulib-imtui-terminal](mulib-imtui-terminal.md), [mulib-imtui-widgets](mulib-imtui-widgets.md), [mulib-strbuf](mulib-strbuf.md) |
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
