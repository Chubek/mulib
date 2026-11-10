# MuLIB lexical-token

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/lexical-token.c)

`mulib_lexical_token_t` contains a kind, borrowed lexeme pointer, byte length,
and source byte offset. Kinds cover EOF, identifier, integer, string, operator,
and lexical error. The module owns no source storage.

`mulib_lexical_token_is` compares the complete token slice to a NUL-terminated
literal, independent of kind. NULL token/literal fails; NULL token text is valid
for zero length and compares equal to an empty string. Nonempty tokens require
valid readable bytes. Complexity is O(literal length + compared bytes).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-lexical-token.h`](../include/mulib-lexical-token.h) |
| Implementation switch | `MULIB_IMPL_LEXICAL_TOKEN` |
| Repository-local includes | None |
| Complete example | [`examples/modules/lexical-token.c`](../examples/modules/lexical-token.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/lexical-token.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
