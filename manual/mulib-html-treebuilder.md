# html-treebuilder

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Owned bounded HTML element/text tree with basic optional-end-tag recovery.

## Contract and supported profile

Parse constructs an owned #document root with linked element/text nodes,
copied raw tag/attribute bytes, copied raw text and source offsets. Comments and
declarations are omitted; entities remain encoded. Void/self-closing elements
do not push depth; repeated active li/p/option elements close the prior peer.
Explicit positive node/depth limits are required (depth<=1024); root is excluded
from the node count. Empty input creates an empty document. On failure out is
unchanged and the partial tree is freed. Successful out must be fresh storage;
free the old tree before replacing it. Not a WHATWG tree builder or native DOM.

Depth counts nested element levels, excluding the synthetic document root and
text nodes. Thus depth one permits `<p>text</p>` but not `<p><b>text</b></p>`.
The independent node budget still counts every element and text node.

## Integration and ownership

Include `include/mulib-html-treebuilder.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_HTML_TEXTSTREAM`, `MULIB_IMPL_HTML_TOKENIZER`, `MULIB_IMPL_HTML_TREEBUILDER`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/html-treebuilder.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_extraction.c`](../tests/test_web_extraction.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-html-treebuilder.h`](../include/mulib-html-treebuilder.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-html-treebuilder.h
 * @brief Owned bounded HTML element/text tree with basic optional-end-tag
 * recovery. C11 single-header module. Define MULIB_IMPL_HTML_TREEBUILDER in
 * exactly one C translation unit. Not a full WHATWG HTML5 tree builder;
 * supported subset is explicitly bounded.
 */
#ifndef MULIB_HTML_TREEBUILDER_H
#define MULIB_HTML_TREEBUILDER_H
#include "mulib-html-tokenizer.h"
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_html_tree_node
  {
    char tag[64];
    mulib_shylock_buffer_t text, attributes;
    struct mulib_html_tree_node *parent, *first_child, *last_child,
        *next_sibling;
    size_t offset;
    int is_text;
  } mulib_html_tree_node_t;
  /** @brief See manual/mulib-html-treebuilder.md for this operation's
   * contract. */
  int mulib_html_tree_parse (const char *html, size_t n, size_t max_nodes,
                             size_t max_depth, mulib_html_tree_node_t **out);
  /** @brief See manual/mulib-html-treebuilder.md for this operation's
   * contract. */
  void mulib_html_tree_free (mulib_html_tree_node_t *root);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HTML_TREEBUILDER_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-html-treebuilder.h`](../include/mulib-html-treebuilder.h) |
| Implementation switch | `MULIB_IMPL_HTML_TREEBUILDER` |
| Repository-local includes | [mulib-html-tokenizer](mulib-html-tokenizer.md), [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/html-treebuilder.c`](../examples/modules/html-treebuilder.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/html-treebuilder.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
