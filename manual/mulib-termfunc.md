# termfunc — Semantic terminal capabilities

Use `mulib_termfunc_emit` to append semantic terminal actions to a buffer.
Supply explicit [terminal capabilities](mulib-terminfo.md); the API never
looks up a terminal or writes a file descriptor. Coordinates are zero-based,
and `move` takes column then row. `region` uses inclusive top and bottom.

Bracketed paste and SGR mouse reporting are xterm extensions and require
`MULIB_TERMFUNC_XTERM_MODES`. See the
[terminal profile](../.sot/term/termscript-profile.md). Unsupported operations
do not leave partial output. Render untrusted text through
[termstyle](mulib-termstyle.md), not through a terminal control path.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-termfunc.h`](../include/mulib-termfunc.h) |
| Implementation switch | `MULIB_IMPL_TERMFUNC` |
| Repository-local includes | [mulib-terminfo](mulib-terminfo.md) |
| Complete example | [`examples/termscript/support.c`](../examples/termscript/support.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/termscript/support.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
