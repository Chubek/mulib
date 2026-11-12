# webextract-rules

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Executable bounded HTML element/tag/class/id text extraction rules.

## Contract and supported profile

A rule contains one terminated simple selector (type/#id/.class/*) and an
optional output-byte limit (default 65536). Extraction parses at most 100000
nodes/depth 128 and selects the first matching element, appending descendant
raw text while omitting script/style subtrees. Nested/void elements are handled
by the shared tree model. Text is not entity-decoded or whitespace-normalized.
No match, malformed input or limits return zero and leave output unchanged.
This profile does not implement compound CSS selectors or multiple rule sets.

## Integration and ownership

Include `include/mulib-webextract-rules.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_DOM_QUERY`, `MULIB_IMPL_HTML_TEXTSTREAM`, `MULIB_IMPL_HTML_TOKENIZER`, `MULIB_IMPL_HTML_TREEBUILDER`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`, `MULIB_IMPL_WEBEXTRACT_RULES`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/webextract-rules.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_extraction.c`](../tests/test_web_extraction.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-webextract-rules.h`](../include/mulib-webextract-rules.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-webextract-rules.h
 * @brief Executable bounded HTML element/tag/class/id text extraction rules.
 * C11 single-header module. Define MULIB_IMPL_WEBEXTRACT_RULES in exactly one
 * C translation unit.
 *
 */
#ifndef MULIB_WEBEXTRACT_RULES_H
#define MULIB_WEBEXTRACT_RULES_H
#include "mulib-dom-query.h"
#include "mulib-html-textstream.h"
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_webextract_rule
  {
    char selector[128];
    size_t max_bytes;
  } mulib_webextract_rule_t;
  /** @brief See manual/mulib-webextract-rules.md for this operation's
   * contract. */
  int mulib_webextract_rules_text (const char *html, size_t n,
                                   const mulib_webextract_rule_t *rule,
                                   mulib_shylock_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_WEBEXTRACT_RULES_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-webextract-rules.h`](../include/mulib-webextract-rules.h) |
| Implementation switch | `MULIB_IMPL_WEBEXTRACT_RULES` |
| Repository-local includes | [mulib-dom-query](mulib-dom-query.md), [mulib-html-textstream](mulib-html-textstream.md), [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/webextract-rules.c`](../examples/modules/webextract-rules.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/webextract-rules.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
