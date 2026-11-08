# mulib-visitor

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_VISITOR`. Traversal itself requires only AST declarations.

`mulib_visitor_walk(root,callback,user)` visits nodes in preorder. A callback
returns `MULIB_VISIT_DESCEND`, `MULIB_VISIT_SKIP` (skip this node's descendants),
or `MULIB_VISIT_STOP`. The walk returns 1 complete, 0 stopped, or -1 for invalid
arguments or an invalid callback result. Sibling traversal continues after SKIP.

Traversal borrows the tree and uses no allocation or recursive calls. It stays
within the given root, even if that root is attached. Do not mutate the tree
inside callbacks. Use listeners when paired enter/leave callbacks are needed.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-visitor.h`](../include/mulib-visitor.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-visitor.h
 * @brief Controllable preorder AST traversal. Define MULIB_IMPL_VISITOR.
 */
#ifndef MULIB_VISITOR_H
#define MULIB_VISITOR_H
#include "mulib-ast.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum mulib_visit_result
  {
    MULIB_VISIT_STOP,
    MULIB_VISIT_DESCEND,
    MULIB_VISIT_SKIP
  } mulib_visit_result_t;
  typedef mulib_visit_result_t (*mulib_visitor_fn) (const mulib_ast_t *node,
                                                    void *user);
  /** Returns 1 complete, 0 stopped, -1 invalid callback/result. */
  int mulib_visitor_walk (const mulib_ast_t *root, mulib_visitor_fn visit,
                          void *user);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-visitor.h`](../include/mulib-visitor.h) |
| Implementation switch | `MULIB_IMPL_VISITOR` |
| Repository-local includes | [mulib-ast](mulib-ast.md) |
| Complete example | [`examples/modules/visitor.c`](../examples/modules/visitor.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/visitor.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
