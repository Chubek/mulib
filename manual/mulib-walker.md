# mulib-walker

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_WALKER`, `MULIB_IMPL_EVTLISTENER` and `MULIB_IMPL_EVTSTREAM`
once; include `mulib-walker.h`. No AST implementation or allocation is required unless the
application uses AST constructors. See `examples/modules/walker.c` and
`tests/test_vizigen.c`.

`mulib_walker_walk(root, &listener)` is an allocation-free visitor walker over
borrowed `mulib_ast_t` trees. It visits each node with ENTER before its children
and LEAVE afterward, in child order. The callback interface is
`mulib_evtlistener_t` from `mulib-evtlistener.h`, so parser-time event
consumers and AST visitors can share callbacks or `mulib-vizigen` bytecode.

The event includes the borrowed node and zero-based depth. Attached subtrees
are bounded to the supplied root and start at depth zero. NULL root is an
empty traversal. Returns 1 on completion, 0 if a callback stops, -1 on an
invalid listener or callback error. No synthetic leave events are emitted
after cancellation. A listener requires a non-NULL callback even for empty
trees. Callbacks normalize positive results to 1 and negative results to -1.

The tree and listener must remain alive and immutable during a walk. Inputs
must be well-formed ASTs with consistent parent/child/next links; this walker
does not validate arbitrary graphs or detect cycles. No ownership transfers,
storage limits or recursive C stack usage are introduced by the walker.
For preorder skip/stop controls rather than paired events see
`mulib-visitor.h`; for existing postorder action tables see `mulib-ast.h`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-walker.h`](../include/mulib-walker.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-walker.h
 * @brief Visitor walker sharing the parser event-listener interface.
 * Define MULIB_IMPL_WALKER, MULIB_IMPL_EVTLISTENER and MULIB_IMPL_EVTSTREAM.
 */
#ifndef MULIB_WALKER_H
#define MULIB_WALKER_H
#include "mulib-evtlistener.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Walk a borrowed AST/subtree in enter/leave order, without allocation.
 * NULL root is empty. Returns 1 complete, 0 callback stopped, -1 error.
 * Neither callbacks nor other threads may mutate the tree during traversal. */
int mulib_walker_walk(const mulib_ast_t *, const mulib_evtlistener_t *);
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
| Header | [`include/mulib-walker.h`](../include/mulib-walker.h) |
| Implementation switch | `MULIB_IMPL_WALKER` |
| Repository-local includes | [mulib-evtlistener](mulib-evtlistener.md) |
| Complete example | [`examples/modules/walker.c`](../examples/modules/walker.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/walker.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
