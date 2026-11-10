# mulib-visitor

## Node-bounded traversal

`mulib_visitor_walk_bounded` calls the preorder visitor at most `max_nodes` times.
Returns 1 complete, 0 callback STOP, -1 invalid input/result, or -2 exhausted.
SKIP counts the current node but excludes its descendants. An exact budget
succeeds if traversal finishes; zero invokes no callbacks. Earlier callbacks
retain their effects on failure: traversal is not failure-atomic. The original
walk is the unlimited wrapper. Input must be a valid borrowed AST, unmodified
during traversal. Traversal stops at the specified root's subtree boundary.

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
/* MuLIB compiler tools: Preorder AST visitor with bounded traversal.
 * ISO C11 single-header module. Define MULIB_IMPL_VISITOR in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_VISITOR_H
#define MULIB_VISITOR_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-ast.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-ast.h"
typedef enum mulib_visit_result {MULIB_VISIT_STOP,MULIB_VISIT_DESCEND,MULIB_VISIT_SKIP} mulib_visit_result_t;
typedef mulib_visit_result_t (*mulib_visitor_fn)(const mulib_ast_t *,void *);
/* Original API, 1 complete, 0 stopped, -1 invalid. */
int mulib_visitor_walk(const mulib_ast_t *root,mulib_visitor_fn visit,void *user);
/* Additional API: -2 when node budget is exhausted. Prior callbacks are retained.
 * Trees are borrowed, valid ASTs and must not be mutated during traversal. */
int mulib_visitor_walk_bounded(const mulib_ast_t *root,mulib_visitor_fn visit,void *user,size_t max_nodes);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_VISITOR_H */
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
