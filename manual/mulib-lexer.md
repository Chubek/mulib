# MuLIB lexer

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/lexer.c)

`init` borrows a byte buffer and resets offset (NULL input initializes empty).
`next` returns 1 for a token, 0 for EOF, or -1 for invalid state/arguments. EOF
has NULL text, zero length, and offset equal to source length; repeated EOF is
stable. Tokens borrow slices and need not be NUL-terminated.

Identifiers use ASCII letters/underscore followed by those or digits; integers
are decimal digit runs. Whitespace space/tab/CR/LF and // or nonnested /* */
comments are skipped. Single/double quoted strings retain delimiters/escapes;
backslash protects the next byte. Unterminated strings/comments yield an ERROR
token and consume the rest. ==, !=, <=, >=, &&, || are paired operators; other
bytes are single operators, including embedded NUL/non-ASCII bytes. This is a
byte lexer, not Unicode identifier processing. Total scan time O(source length).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-lexer.h`](../include/mulib-lexer.h) |
| Implementation switch | `MULIB_IMPL_LEXER` |
| Repository-local includes | [mulib-lexical-token](mulib-lexical-token.md) |
| Complete example | [`examples/modules/lexer.c`](../examples/modules/lexer.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/lexer.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
