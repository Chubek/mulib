# MuLIB lexgen

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/lexgen.c)

`match` evaluates borrowed rules at the beginning of a byte slice and selects the
longest nonempty match, breaking ties by rule order. Rules are NUL-terminated
literal strings, ASCII decimal digit runs, ASCII identifiers (letter/underscore
then letters/digits/underscore), or space/tab/CR/LF runs. Token IDs are arbitrary
caller-supplied integers. Empty literals do not match.

Return 1 writes matched length and token; 0 means no match; -1 means invalid
arguments/rule kind or NULL literal. Outputs remain unchanged on 0/-1. The API
does not advance input or skip whitespace automatically. NULL text/rules are
allowed at zero corresponding lengths. Time is O(total rule scan work), with
no allocation, generated automaton, regular expressions, or hidden state.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-lexgen.h`](../include/mulib-lexgen.h) |
| Implementation switch | `MULIB_IMPL_LEXGEN` |
| Repository-local includes | None |
| Complete example | [`examples/modules/lexgen.c`](../examples/modules/lexgen.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/lexgen.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
