# termscript-terminal — Terminal host for Termscript

The adapter sends complete output slices to an explicit writer callback.
It accepts safe UTF-8 text separately from semantic Termfunc operations and
trusted Terminfo capabilities. It can write to the repository's
[VTerm](mulib-vterm.md) without an actual terminal.

An application may allow POSIX raw mode by supplying an input descriptor.
The adapter borrows that descriptor; restore its state before closing it.
Restore errors are reported so the caller can retry. There is no signal
handler or hidden cleanup. Terminal foreground process-group and job-control
policy belongs to the application.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-termscript-terminal.h`](../kernels/mulib-termscript-terminal.h) |
| Implementation switch | `MULIB_IMPL_TERMSCRIPT_TERMINAL` |
| Repository-local includes | [mulib-termfunc](mulib-termfunc.md), [mulib-termstyle](mulib-termstyle.md), [mulib-termutils](mulib-termutils.md) |
| Complete example | [`examples/termscript/support.c`](../examples/termscript/support.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/termscript/support.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
