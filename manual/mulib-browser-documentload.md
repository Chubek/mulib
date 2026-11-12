# browser-documentload

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Synchronous document fetch and HTML tree creation orchestration.

## Contract and supported profile

Load synchronously fetches and parses successful 2xx text/html responses;
media type is case-insensitive with a proper delimiter. XHTML is not treated as
HTML. The basic call uses depth 128 and response ceiling 16 MiB; bounded also
takes positive depth<=1024 and byte budgets, further restricted by fetch policy.
Successful output owns a new tree and receives status; both are unchanged on
failure. Caller output must be fresh/released storage. Empty HTML creates a
document root. Subresources, charset transcoding, scripts, redirects and CSS
processing are not performed here.

## Integration and ownership

Include `include/mulib-browser-documentload.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_AST`, `MULIB_IMPL_BROWSER_DOCUMENTLOAD`, `MULIB_IMPL_BROWSER_FETCH`, `MULIB_IMPL_BROWSER_ORIGIN`, `MULIB_IMPL_FETCH`, `MULIB_IMPL_FETCH_POLICY`, `MULIB_IMPL_GLOB`, `MULIB_IMPL_HTML_TEXTSTREAM`, `MULIB_IMPL_HTML_TOKENIZER`, `MULIB_IMPL_HTML_TREEBUILDER`, `MULIB_IMPL_INET4`, `MULIB_IMPL_INET6`, `MULIB_IMPL_INTE6`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_PARSER`, `MULIB_IMPL_PEG`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_URI`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/browser-documentload.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-browser-documentload.h`](../include/mulib-browser-documentload.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-browser-documentload.h
 * @brief Synchronous document fetch and HTML tree creation orchestration.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_DOCUMENTLOAD in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_BROWSER_DOCUMENTLOAD_H
#define MULIB_BROWSER_DOCUMENTLOAD_H
#include "mulib-browser-fetch.h"
#include "mulib-html-treebuilder.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-browser-documentload.md for this operation's
   * contract. */
  int mulib_browser_documentload (const mulib_browser_fetch_t *fetch,
                                  const char *url, size_t max_nodes,
                                  mulib_html_tree_node_t **root,
                                  unsigned *status);
  /** @brief Load with explicit tree depth and response byte budgets. */
  int mulib_browser_documentload_bounded (const mulib_browser_fetch_t *fetch,
                                          const char *url, size_t max_nodes,
                                          size_t max_depth, size_t max_bytes,
                                          mulib_html_tree_node_t **root,
                                          unsigned *status);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_DOCUMENTLOAD_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browser-documentload.h`](../include/mulib-browser-documentload.h) |
| Implementation switch | `MULIB_IMPL_BROWSER_DOCUMENTLOAD` |
| Repository-local includes | [mulib-browser-fetch](mulib-browser-fetch.md), [mulib-html-treebuilder](mulib-html-treebuilder.md) |
| Complete example | [`examples/modules/browser-documentload.c`](../examples/modules/browser-documentload.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browser-documentload.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
