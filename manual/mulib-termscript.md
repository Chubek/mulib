# termscript — Stack language for terminal applications

Termscript combines a stack machine with explicit terminal capabilities.
The parser uses `mulib-parsec` and preserves source byte offsets. The grammar
and operation semantics are documented here; terminal escape sequences are
profiled against [`.sot/term`](../.sot/term/termscript-profile.md).

Words execute against the stack. Integers, quoted strings, and characters
push values; backtick literals are command data, not evaluation. Use `#name`
for symbols, `@name` for array symbols, `%name` for record symbols, `$name`
for bound parameters, and `!name` for keys. `#\newline` is a character.
Braces quote code. Tuples use parentheses, arrays square brackets, and
records angle brackets with `name: value` fields. Whitespace, commas and
semicolons separate terms. A `%` followed by whitespace starts a line comment.

`[[CUP]]` is a first-class escape value, not an embedded control byte. `emit`
resolves its name against the host's explicit [terminal database](mulib-termdb.md)
and validates the parameters before writing. For example, `[[kf11]] emit`
has no parameters and requires the explicit xterm profile. `(2, 4) [[CUP]] emit`
uses zero-based row then column; the semantic `move` word instead takes column
then row.

```termscript
%module: demo.counter
#i 0 set;
{ #i 10 lt? } while { #i 1 add; } loop;
#i typeout; #\newline typeout;
```

Use `0 4 iota` for an inclusive array range, `5 @items push` to append,
and `@items pop` to remove and return the last value. A literal initializer
such as `@items { 0 4 iota } set` must produce exactly one value. `bind`
defines a callable quote instead: `#sum ($a, $b) { $a $b add } bind seal`
defines `sum`. Use `!f1 { "help" typeout } bind` in the current mode.
`!f1 press` dispatches locally; it does not synthesize terminal input.
`%if-term: xterm*`, `%else:`, and `%end:` select code using the host's `TERM`.

The evaluator is bounded by configurable execution and storage limits.
Applications choose whether imports, terminfo sequences, POSIX termios, and
command handlers are available. Module paths are resolved only under trusted
roots. For `exec`, the command literal alone never grants access to a shell.

The compiler canonicalizes the parsed representation before emitting C11.
The resulting C is callable with the same native host as interpreted code.
It embeds the executable AST; operation and control-flow execution still
uses the bounded runtime.

```sh
make build/termscript
./build/termscript library/termscript/examples/counter.term
./build/termscript --compile counter library/termscript/examples/counter.term > build/counter.c
python3 tools/mk-termdb.py build/terminal.sqlite3
./build/termscript --termdb build/terminal.sqlite3 library/termscript/examples/escape.term
```

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-termscript.h`](../include/mulib-termscript.h) |
| Implementation switch | `MULIB_IMPL_TERMSCRIPT` |
| Repository-local includes | [mulib-termscript-compiler](mulib-termscript-compiler.md), [mulib-termscript-host](mulib-termscript-host.md) |
| Complete example | [`library/termscript/main.c`](../library/termscript/main.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude library/termscript/main.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
