# Termscript standard library

Termscript is a stack language for programs driven by terminal state and events.
Its syntax and capability profile are described in the
[Termscript manual](../../manual/mulib-termscript.md) and the in-tree
[terminal references](../../.sot/term/termscript-profile.md).

A dotted module name such as `stdlib.terminfo` denotes `stdlib/terminfo.term`.
Termscript checks the colon-separated `TERMSCRIPT_LIBRARY_PATH` first, then
the standard library root supplied by the host. It ignores empty path
elements, not treating them as the current directory. Use only trusted roots.
Importing a module can execute its code; neither source directives nor a
backtick literal grant permission to use system resources.

The standard library includes semantic operations (`stdlib.termfunc`),
the short capability-name namespace (`stdlib.terminfo`), a VTerm output
adapter (`stdlib.vterm`), text/control helpers, and state-machine operations.
On platforms without POSIX terminals, scripts can still use a buffered or
VTerm output host; the termios operations are unavailable.
