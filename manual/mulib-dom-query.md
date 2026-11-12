# dom-query

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Bounded tag, ID, and class lookups over owned HTML tree nodes.

## Contract and supported profile

Queries walk an immutable HTML-tree forest in preorder, counting the root
against a positive max_nodes budget but not matching #document/text nodes.
Supported selectors are a single type, #id, .class or *. Type and attribute names
are ASCII-case-insensitive; ID/class values are case-sensitive and class matches
whole whitespace-separated tokens. No compounds/combinators/pseudo-classes.
`all` returns the total visited matches, stores at most cap borrowed pointers,
and permits NULL output for counting. Budget exhaustion truncates the search,
not a distinct error. `first` returns a borrowed node or NULL.

## Integration and ownership

Include `include/mulib-dom-query.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_DOM_QUERY`, `MULIB_IMPL_HTML_TEXTSTREAM`, `MULIB_IMPL_HTML_TOKENIZER`, `MULIB_IMPL_HTML_TREEBUILDER`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/dom-query.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_extraction.c`](../tests/test_web_extraction.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-dom-query.h`](../include/mulib-dom-query.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-dom-query.h
 * @brief Bounded tag, ID, and class lookups over owned HTML tree nodes.
 * C11 single-header module. Define MULIB_IMPL_DOM_QUERY in exactly one C
 * translation unit. Supports single simple selectors only, not full Selectors
 * Level 4.
 */
#ifndef MULIB_DOM_QUERY_H
#define MULIB_DOM_QUERY_H
#include "mulib-html-treebuilder.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  const mulib_html_tree_node_t *
  /** @brief See manual/mulib-dom-query.md for this operation's contract. */
  mulib_dom_query_first (const mulib_html_tree_node_t *root,
                         const char *selector, size_t max_nodes);
  /** @brief Match a single type, #id, .class or * selector; no combinators. */
  int mulib_dom_query_matches (const mulib_html_tree_node_t *node,
                               const char *selector);
  /** @brief See manual/mulib-dom-query.md for this operation's contract. */
  size_t mulib_dom_query_all (const mulib_html_tree_node_t *root,
                              const char *selector,
                              const mulib_html_tree_node_t **out, size_t cap,
                              size_t max_nodes);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DOM_QUERY_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dom-query.h`](../include/mulib-dom-query.h) |
| Implementation switch | `MULIB_IMPL_DOM_QUERY` |
| Repository-local includes | [mulib-html-treebuilder](mulib-html-treebuilder.md) |
| Complete example | [`examples/modules/dom-query.c`](../examples/modules/dom-query.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dom-query.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
