# css-cascade

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Deterministic selected-property cascade with basic specificity and source ordering.

## Contract and supported profile

Compute starts with defaults, inherits only color/font-size from an optional
initialized parent, then applies matching flat author rules in ascending simple
specificity, preserving source order within each level. It rejects unsupported
selectors rather than silently ignoring their syntax. Only css-computed's five
properties participate; !important, inline styles, layers, origins and general
inheritance are not supported. Inputs are borrowed; a complete result commits
only on success. No rendering or element-default display model is inferred.

## Integration and ownership

Include `include/mulib-css-cascade.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_CSS_CASCADE`, `MULIB_IMPL_CSS_COMPUTED`, `MULIB_IMPL_CSS_PARSER`, `MULIB_IMPL_CSS_SELECTOR`, `MULIB_IMPL_DOM_QUERY`, `MULIB_IMPL_HTML_TEXTSTREAM`, `MULIB_IMPL_HTML_TOKENIZER`, `MULIB_IMPL_HTML_TREEBUILDER`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/css-cascade.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_css.c`](../tests/test_web_css.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-css-cascade.h`](../include/mulib-css-cascade.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-css-cascade.h
 * @brief Deterministic selected-property cascade with basic specificity and
 * source ordering. C11 single-header module. Define MULIB_IMPL_CSS_CASCADE in
 * exactly one C translation unit. Supports a deliberately small cascade
 * subset, without CSS layers or full inheritance.
 */
#ifndef MULIB_CSS_CASCADE_H
#define MULIB_CSS_CASCADE_H
#include "mulib-css-computed.h"
#include "mulib-css-parser.h"
#include "mulib-css-selector.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-css-cascade.md for this operation's contract. */
  int mulib_css_cascade_compute (const mulib_css_parsed_sheet_t *sheet,
                                 const mulib_html_tree_node_t *node,
                                 const mulib_css_computed_t *parent,
                                 mulib_css_computed_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSS_CASCADE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-css-cascade.h`](../include/mulib-css-cascade.h) |
| Implementation switch | `MULIB_IMPL_CSS_CASCADE` |
| Repository-local includes | [mulib-css-computed](mulib-css-computed.md), [mulib-css-parser](mulib-css-parser.md), [mulib-css-selector](mulib-css-selector.md) |
| Complete example | [`examples/modules/css-cascade.c`](../examples/modules/css-cascade.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/css-cascade.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
