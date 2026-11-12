# css-selector

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Simple type, ID, class and universal selector matching for HTML tree nodes.

## Contract and supported profile

Matching delegates to DOM-query's single type/#id/.class/* grammar, sharing
exact attribute parsing and class-token semantics. Text/#document nodes never
match; type names ignore ASCII case, ID/class values do not. Specificity returns
0 for *, 1 for type, 10 for class, 100 for ID, and -1 for unsupported syntax.
Compounds, lists, combinators, escapes and pseudo-classes are not implemented.
Inputs borrow immutable HTML-tree nodes; no allocation or mutation occurs.

## Integration and ownership

Include `include/mulib-css-selector.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_CSS_SELECTOR`, `MULIB_IMPL_DOM_QUERY`, `MULIB_IMPL_HTML_TEXTSTREAM`, `MULIB_IMPL_HTML_TOKENIZER`, `MULIB_IMPL_HTML_TREEBUILDER`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/css-selector.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_css.c`](../tests/test_web_css.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-css-selector.h`](../include/mulib-css-selector.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-css-selector.h
 * @brief Simple type, ID, class and universal selector matching for HTML tree
 * nodes. C11 single-header module. Define MULIB_IMPL_CSS_SELECTOR in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_CSS_SELECTOR_H
#define MULIB_CSS_SELECTOR_H
#include "mulib-dom-query.h"
#include "mulib-html-treebuilder.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-css-selector.md for this operation's contract. */
  int mulib_css_selector_matches (const mulib_html_tree_node_t *node,
                                  const char *selector);
  /** @brief See manual/mulib-css-selector.md for this operation's contract. */
  int mulib_css_selector_specificity (const char *selector);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSS_SELECTOR_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-css-selector.h`](../include/mulib-css-selector.h) |
| Implementation switch | `MULIB_IMPL_CSS_SELECTOR` |
| Repository-local includes | [mulib-dom-query](mulib-dom-query.md), [mulib-html-treebuilder](mulib-html-treebuilder.md) |
| Complete example | [`examples/modules/css-selector.c`](../examples/modules/css-selector.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/css-selector.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
