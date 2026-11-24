# termscript-host — Standard VM capabilities

Attach the standard host with an explicit terminal, optional database and
trusted library roots. The host supplies text, semantic `term.*` operations,
`emit`, POSIX `termios.*` operations, and finite-state transition helpers.
A backtick string is never executed directly by this host. `exec` dispatches
capability namespaces or calls a separately supplied command callback.

Use `--allow-shell` on the command-line runner only for source and imports
you trust. Ordinary `typeout` replaces control characters.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-termscript-host.h`](../kernels/mulib-termscript-host.h) |
| Implementation switch | `MULIB_IMPL_TERMSCRIPT_HOST` |
| Repository-local includes | [mulib-termdb](mulib-termdb.md), [mulib-termscript-library](mulib-termscript-library.md), [mulib-termscript-runtime](mulib-termscript-runtime.md), [mulib-termscript-state](mulib-termscript-state.md), [mulib-termscript-terminal](mulib-termscript-terminal.md), [mulib-utf8](mulib-utf8.md) |
| Complete example | [`library/termscript/main.c`](../library/termscript/main.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels library/termscript/main.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
