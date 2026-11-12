# readability

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Bounded semantic plain-text extraction from HTML with basic entity decoding.

## Contract and supported profile

Extract appends plain text after omitting comments and script/style/noscript
subtrees, collapsing whitespace and separating tag boundaries. It decodes only
amp/lt/gt/quot/apos/nbsp named entities; numeric and unknown entities remain
literal. It is semantic text cleanup, not article ranking, sanitization, Unicode
normalization or rendering. A one-million-event bound and output buffer limit
apply. Extraction is staged, so malformed input or exhaustion preserves caller
contents. No pointers into input survive the call.

## Integration and ownership

Include `include/mulib-readability.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_HTML_TEXTSTREAM`, `MULIB_IMPL_READABILITY`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/readability.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_extraction.c`](../tests/test_web_extraction.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-readability.h`](../include/mulib-readability.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-readability.h
 * @brief Bounded semantic plain-text extraction from HTML with basic entity
 * decoding. C11 single-header module. Define MULIB_IMPL_READABILITY in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_READABILITY_H
#define MULIB_READABILITY_H
#include "mulib-html-textstream.h"
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-readability.md for this operation's contract. */
  int mulib_readability_extract (const char *html, size_t length,
                                 mulib_shylock_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_READABILITY_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-readability.h`](../include/mulib-readability.h) |
| Implementation switch | `MULIB_IMPL_READABILITY` |
| Repository-local includes | [mulib-html-textstream](mulib-html-textstream.md), [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/readability.c`](../examples/modules/readability.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/readability.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
