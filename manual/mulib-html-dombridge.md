# html-dombridge

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Tree walker with caller-provided native DOM construction callbacks.

## Contract and supported profile

Walk visits the supplied node and its sibling forest in preorder, calling
make(ctx,node,native_parent) for every node, then optional text for text nodes,
then descendants with the returned native handle. Handles and input nodes are
borrowed; the application owns and cleans up any native allocations. Positive
depth_limit includes the initial level; zero, missing make/root, NULL handles
or text callback zero stop with failure. Already-created native state is not
rolled back. Input must be an acyclic tree within a safe recursion budget.

## Integration and ownership

Include `include/mulib-html-dombridge.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_HTML_DOMBRIDGE`, `MULIB_IMPL_HTML_TEXTSTREAM`, `MULIB_IMPL_HTML_TOKENIZER`, `MULIB_IMPL_HTML_TREEBUILDER`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/html-dombridge.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_extraction.c`](../tests/test_web_extraction.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-html-dombridge.h`](../include/mulib-html-dombridge.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-html-dombridge.h
 * @brief Tree walker with caller-provided native DOM construction callbacks.
 * C11 single-header module. Define MULIB_IMPL_HTML_DOMBRIDGE in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_HTML_DOMBRIDGE_H
#define MULIB_HTML_DOMBRIDGE_H
#include "mulib-html-treebuilder.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_html_dombridge_ops
  {
    void *(*make) (void *ctx, const mulib_html_tree_node_t *node,
                   void *parent);
    int (*text) (void *ctx, void *node, const char *text, size_t length);
  } mulib_html_dombridge_ops_t;
  /** @brief See manual/mulib-html-dombridge.md for this operation's contract.
   */
  int mulib_html_dombridge_walk (const mulib_html_tree_node_t *root,
                                 const mulib_html_dombridge_ops_t *ops,
                                 void *ctx, void *parent, size_t depth_limit);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HTML_DOMBRIDGE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-html-dombridge.h`](../include/mulib-html-dombridge.h) |
| Implementation switch | `MULIB_IMPL_HTML_DOMBRIDGE` |
| Repository-local includes | [mulib-html-treebuilder](mulib-html-treebuilder.md) |
| Complete example | [`examples/modules/html-dombridge.c`](../examples/modules/html-dombridge.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/html-dombridge.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
