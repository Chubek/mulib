# html-textstream

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Length-aware tokenizer-style HTML tag/text/comment callbacks with source offsets.

## Contract and supported profile

Scan emits borrowed source spans in lexical order with byte offsets for
start/end tags, text and comments (including declarations). It respects quoted
attributes and treats script/style/title/textarea contents as raw text until a
matching case-insensitive end tag. Unterminated recognized tags/comments/raw text
and embedded NUL fail; arbitrary less-than text is retained. max_events zero is
unlimited. Empty NULL/zero input succeeds without callbacks. Callback zero stops
with failure, and previously delivered callbacks cannot be rolled back. No
entity decoding, incremental feed state or full HTML5 recovery is promised.

## Integration and ownership

Include `include/mulib-html-textstream.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_HTML_TEXTSTREAM`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/html-textstream.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_extraction.c`](../tests/test_web_extraction.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-html-textstream.h`](../include/mulib-html-textstream.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-html-textstream.h
 * @brief Length-aware tokenizer-style HTML tag/text/comment callbacks with
 * source offsets. C11 single-header module. Define MULIB_IMPL_HTML_TEXTSTREAM
 * in exactly one C translation unit.
 *
 */
#ifndef MULIB_HTML_TEXTSTREAM_H
#define MULIB_HTML_TEXTSTREAM_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum mulib_html_textstream_kind
  {
    MULIB_HTML_STREAM_START = 1,
    MULIB_HTML_STREAM_END = 2,
    MULIB_HTML_STREAM_TEXT = 3,
    MULIB_HTML_STREAM_COMMENT = 4
  } mulib_html_textstream_kind_t;
  typedef struct mulib_html_textstream_event
  {
    mulib_html_textstream_kind_t kind;
    const char *start;
    size_t len, offset;
  } mulib_html_textstream_event_t;
  typedef int (*mulib_html_textstream_fn) (
      void *user, const mulib_html_textstream_event_t *event);
  /** @brief See manual/mulib-html-textstream.md for this operation's contract.
   */
  int mulib_html_textstream_scan (const char *html, size_t length,
                                  mulib_html_textstream_fn callback, void *ctx,
                                  size_t max_events);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HTML_TEXTSTREAM_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-html-textstream.h`](../include/mulib-html-textstream.h) |
| Implementation switch | `MULIB_IMPL_HTML_TEXTSTREAM` |
| Repository-local includes | None |
| Complete example | [`examples/modules/html-textstream.c`](../examples/modules/html-textstream.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/html-textstream.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
