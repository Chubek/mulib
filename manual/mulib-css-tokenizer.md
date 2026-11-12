# css-tokenizer

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Length-aware CSS identifiers, signed numbers, strings and symbols with comment skipping.

## Contract and supported profile

Scan emits borrowed IDENT/STRING/NUMBER/SYMBOL spans with byte offsets,
skipping whitespace and closed block comments. Numbers include signed decimal
fractions and optional decimal exponents; units and percent are separate tokens.
Strings retain quotes and escaped bytes. Unterminated comments/strings and
embedded NUL fail. Empty NULL/zero input succeeds; zero token limit is unlimited.
Callback zero fails after earlier deliveries. This is not CSS Syntax Level 3:
ident escapes are not decoded, and tokens are not semantic CSS values.

## Integration and ownership

Include `include/mulib-css-tokenizer.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_CSS_TOKENIZER`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/css-tokenizer.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_css.c`](../tests/test_web_css.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-css-tokenizer.h`](../include/mulib-css-tokenizer.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-css-tokenizer.h
 * @brief Length-aware CSS identifiers, numbers, strings and symbols; skips
 * comments. C11 single-header module. Define MULIB_IMPL_CSS_TOKENIZER in
 * exactly one C translation unit. Tokenizes a restricted CSS profile; not a
 * complete CSS Syntax Level 3 tokenizer.
 */
#ifndef MULIB_CSS_TOKENIZER_H
#define MULIB_CSS_TOKENIZER_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum mulib_css_token_kind
  {
    MULIB_CSS_TOKEN_IDENT = 1,
    MULIB_CSS_TOKEN_STRING = 2,
    MULIB_CSS_TOKEN_NUMBER = 3,
    MULIB_CSS_TOKEN_SYMBOL = 4
  } mulib_css_token_kind_t;
  typedef struct mulib_css_token
  {
    mulib_css_token_kind_t kind;
    const char *bytes;
    size_t length, offset;
  } mulib_css_token_t;
  typedef int (*mulib_css_tokenizer_fn) (void *user,
                                         const mulib_css_token_t *token);
  /** @brief See manual/mulib-css-tokenizer.md for this operation's contract.
   */
  int mulib_css_tokenizer_scan (const char *source, size_t n,
                                mulib_css_tokenizer_fn fn, void *ctx,
                                size_t limit);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSS_TOKENIZER_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-css-tokenizer.h`](../include/mulib-css-tokenizer.h) |
| Implementation switch | `MULIB_IMPL_CSS_TOKENIZER` |
| Repository-local includes | None |
| Complete example | [`examples/modules/css-tokenizer.c`](../examples/modules/css-tokenizer.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/css-tokenizer.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
