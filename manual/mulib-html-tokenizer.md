# html-tokenizer

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Length-aware HTML lexical tokens with tag-name and attribute spans.

## Contract and supported profile

Tokenizer adapts textstream events with lowercase ASCII tag names (up to
63 bytes), raw source spans and offsets. It preserves the scanner's raw-text and
malformed-input rules. `html_attribute` borrows the first exact attribute value,
compares attribute names without ASCII case, skips quoted unrelated values and
supports boolean/quoted/unquoted attributes. Values are not entity-decoded;
boolean attributes have length zero. NULL/empty names and malformed tags fail
without changing value/length outputs. This is a lexical subset, not HTML5.

## Integration and ownership

Include `include/mulib-html-tokenizer.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_HTML_TEXTSTREAM`, `MULIB_IMPL_HTML_TOKENIZER`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/html-tokenizer.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_extraction.c`](../tests/test_web_extraction.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-html-tokenizer.h`](../include/mulib-html-tokenizer.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-html-tokenizer.h
 * @brief Length-aware HTML lexical tokens with tag-name and attribute spans.
 * C11 single-header module. Define MULIB_IMPL_HTML_TOKENIZER in exactly one C
 * translation unit. This is a practical lexical subset, not the full WHATWG
 * HTML5 tokenizer.
 */
#ifndef MULIB_HTML_TOKENIZER_H
#define MULIB_HTML_TOKENIZER_H
#include "mulib-html-textstream.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_html_token
  {
    mulib_html_textstream_kind_t kind;
    char name[64];
    const char *raw;
    size_t length, offset;
  } mulib_html_token_t;
  typedef int (*mulib_html_tokenizer_fn) (void *ctx,
                                          const mulib_html_token_t *t);
  /** @brief See manual/mulib-html-tokenizer.md for this operation's contract.
   */
  int mulib_html_tokenizer_scan (const char *html, size_t n,
                                 mulib_html_tokenizer_fn cb, void *ctx,
                                 size_t max_events);
  /** @brief Borrow the first exact, ASCII-case-insensitive attribute value. */
  int mulib_html_attribute (const char *tag, size_t n, const char *name,
                            const char **value, size_t *length);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HTML_TOKENIZER_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-html-tokenizer.h`](../include/mulib-html-tokenizer.h) |
| Implementation switch | `MULIB_IMPL_HTML_TOKENIZER` |
| Repository-local includes | [mulib-html-textstream](mulib-html-textstream.md) |
| Complete example | [`examples/modules/html-tokenizer.c`](../examples/modules/html-tokenizer.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/html-tokenizer.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
