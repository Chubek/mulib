# termdb — Terminal escape database

`python3 tools/mk-termdb.py build/terminal.sqlite3` combines the sources under
[`.sot/term`](../.sot/term/) into a SQLite file. The generator uses only Python's
standard library. The C kernel reads that same file through MuLIB's
[SQLite implementation](mulib-sqlite.md), with no external SQLite library.
If the local reference corpus is not present, supply it under `.sot/term`
or use `--source-dir`. Files the generator cannot parse are archived as
opaque bytes with a warning; a code without matching source evidence is
not available for emission.

Open a trusted generated database explicitly. Escape names and profiles
are case-sensitive. Bind numerical parameters to a template's required
arity; template expansion does not execute a command or write to the
terminal. In Termscript, `[[name]]` is inert data until `emit` is called.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-termdb.h`](../kernels/mulib-termdb.h) |
| Implementation switch | `MULIB_IMPL_TERMDB` |
| Repository-local includes | [mulib-sqlite](mulib-sqlite.md), [mulib-terminfo](mulib-terminfo.md), [mulib-utf8](mulib-utf8.md) |
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
