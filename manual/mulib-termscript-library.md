# termscript-library — Termscript module resolution

`mulib_termscript_library_load` resolves a dotted module name to a `.term`
file under a list of trusted search roots. A `NULL` search path reads
`TERMSCRIPT_LIBRARY_PATH`. Search proceeds left to right, then tries the
explicit standard-library root. Empty path components are ignored.

`my.widgets` maps to `my/widgets.term`. Only identifier-like components
are accepted. There is no path escape through the module name; a trusted
root can still contain symbolic links. The first existing module wins
even if it is invalid. Loading does not execute source, and the resulting
program is owned by the caller.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-termscript-library.h`](../kernels/mulib-termscript-library.h) |
| Implementation switch | `MULIB_IMPL_TERMSCRIPT_LIBRARY` |
| Repository-local includes | [mulib-strbuf](mulib-strbuf.md), [mulib-termscript-ast](mulib-termscript-ast.md) |
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
