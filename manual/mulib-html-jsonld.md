# html-jsonld

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Locate multiple application/ld+json script bodies as borrowed spans.

## Contract and supported profile

Scan reports complete script bodies only when the exact type attribute is
application/ld+json (case-insensitive, surrounding whitespace trimmed). Comments,
data-type attributes and substring matches do not qualify. Bodies/offsets borrow
the original input and are not parsed or decoded. Empty bodies may be delivered.
Zero limit is unlimited; the scanner also caps lexical events at one million.
Malformed HTML/raw text, limit exhaustion or callback zero return failure after
any earlier callbacks. The input must outlive borrowed spans.

## Integration and ownership

Include `include/mulib-html-jsonld.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_HTML_JSONLD`, `MULIB_IMPL_HTML_TEXTSTREAM`, `MULIB_IMPL_HTML_TOKENIZER`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/html-jsonld.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_extraction.c`](../tests/test_web_extraction.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-html-jsonld.h`](../include/mulib-html-jsonld.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-html-jsonld.h
 * @brief Locate multiple application/ld+json script bodies as borrowed spans.
 * C11 single-header module. Define MULIB_IMPL_HTML_JSONLD in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_HTML_JSONLD_H
#define MULIB_HTML_JSONLD_H
#include "mulib-html-tokenizer.h"
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef int (*mulib_html_jsonld_fn) (void *ctx, mulib_shylock_span_t source,
                                       size_t offset);
  /** @brief See manual/mulib-html-jsonld.md for this operation's contract. */
  int mulib_html_jsonld_scan (const char *html, size_t n,
                              mulib_html_jsonld_fn callback, void *ctx,
                              size_t limit);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HTML_JSONLD_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-html-jsonld.h`](../include/mulib-html-jsonld.h) |
| Implementation switch | `MULIB_IMPL_HTML_JSONLD` |
| Repository-local includes | [mulib-html-tokenizer](mulib-html-tokenizer.md), [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/html-jsonld.c`](../examples/modules/html-jsonld.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/html-jsonld.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
