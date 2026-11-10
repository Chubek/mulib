# mulib-listener

## Event-bounded callbacks

`mulib_listener_walk_bounded` limits AST events, not callback invocations. Each
ENTER/LEAVE consumes one event even if no listener matches. Returns 1 complete,
0 callback stop, -1 invalid arguments/traversal, or -2 budget exhausted. A zero
budget invokes nothing. An exact event budget succeeds if traversal completes.
Earlier callback effects remain on stop/error; there is no rollback. NULL
production matches every node; otherwise matching is exact and case-sensitive.
Listener descriptors, contexts, and tree are borrowed and must remain valid
without structural mutation for the walk. The original walk has no event bound.

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_LISTENER` and `MULIB_IMPL_EVTSTREAM` in one translation unit.

`mulib_listener_walk(root,listeners,count)` consumes AST events and dispatches
`mulib_listener_t` entries in table order. Each entry contains a production name
(NULL means all productions), optional `enter` and `leave` callbacks, and user
context. Names match productions, not semantic-action aliases.

Callbacks return nonzero to continue or zero to stop. The walk returns 1 on
completion, 0 on an early stop, -1 for invalid arguments. Early stop does not
unwind pending leave callbacks. Trees, tables and contexts remain borrowed;
callbacks must not change the traversed tree.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-listener.h`](../include/mulib-listener.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB compiler tools: Patched production-specific AST listeners with explicit event budget.
 * ISO C11 single-header module. Define MULIB_IMPL_LISTENER in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_LISTENER_H
#define MULIB_LISTENER_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-evtstream.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-evtstream.h"
typedef struct mulib_listener {const char *production;
mulib_ast_action_fn enter,leave;
void *user;
} mulib_listener_t;
int mulib_listener_walk(const mulib_ast_t *root,const mulib_listener_t *listeners,size_t count);
int mulib_listener_walk_bounded(const mulib_ast_t *root,const mulib_listener_t *listeners,size_t count,size_t max_events);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_LISTENER_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-listener.h`](../include/mulib-listener.h) |
| Implementation switch | `MULIB_IMPL_LISTENER` |
| Repository-local includes | [mulib-evtstream](mulib-evtstream.md) |
| Complete example | [`examples/modules/listener.c`](../examples/modules/listener.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/listener.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
