# mulib-evtstream

## Depth-bounded iteration

`mulib_evtstream_next_bounded` adds a maximum depth (root depth is zero).
It returns -2 if the next transition would descend past that depth; both stream
and event remain unchanged, allowing retry with a larger bound. In particular,
a root with children and bound zero is rejected before its ENTER is emitted.
Otherwise returns 1 for an event, 0 for end, -1 for invalid arguments/links.
The original `next` is the unlimited wrapper. Trees remain borrowed valid ASTs;
do not mutate them during iteration. A subtree root does not visit its siblings.

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_EVTSTREAM`. Includes AST declarations; event iteration itself
has no allocation or link-time AST dependency.

`mulib_evtstream_from_ast(root)` returns a copyable cursor over a borrowed tree.
`mulib_evtstream_next(&cursor,&event)` returns 1 for an event, 0 at exhaustion,
-1 for invalid arguments. An empty/NULL root is an empty stream. Exhaustion is
sticky. Copy a cursor to independently resume traversal at the same position.

Events contain ENTER/LEAVE `kind`, borrowed `node` and zero-based `depth`.
Every node produces an ENTER before its children and a LEAVE after its children.
Traversal is bounded to the supplied root, even for attached subtrees.
The tree must outlive cursors and must not be changed while they are used.

## Push listeners

The listener structure and delivery API live in
[`mulib-evtlistener.h`](mulib-evtlistener.md). Initialize a
`mulib_evtlistener_t` with a callback and user pointer, then call
`mulib_evtlistener_listen(&stream, &listener)` to consume this stream, or
`mulib_evtlistener_push(&listener, &event)` to deliver parser-time events.
Implement `MULIB_IMPL_EVTLISTENER` explicitly when using these APIs.
`mulib-walker.h` and `mulib-vizigen.h` share this listener interface.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-evtstream.h`](../include/mulib-evtstream.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB compiler tools: Patched balanced AST event-stream iteration with depth budgeting.
 * ISO C11 single-header module. Define MULIB_IMPL_EVTSTREAM in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_EVTSTREAM_H
#define MULIB_EVTSTREAM_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-ast.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-ast.h"
typedef enum mulib_event_kind {MULIB_EVENT_ENTER,MULIB_EVENT_LEAVE} mulib_event_kind_t;
typedef struct mulib_event {mulib_event_kind_t kind;
const mulib_ast_t *node;
size_t depth;
} mulib_event_t;
typedef struct mulib_evtstream {const mulib_ast_t *root,*node;
size_t depth;
int leaving;
} mulib_evtstream_t;
mulib_evtstream_t mulib_evtstream_from_ast(const mulib_ast_t *root);
int mulib_evtstream_next(mulib_evtstream_t *stream,mulib_event_t *event);
/* -2 when traversal would exceed maximum depth; state unchanged. */
int mulib_evtstream_next_bounded(mulib_evtstream_t *stream,mulib_event_t *event,size_t maximum_depth);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EVTSTREAM_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-evtstream.h`](../include/mulib-evtstream.h) |
| Implementation switch | `MULIB_IMPL_EVTSTREAM` |
| Repository-local includes | [mulib-ast](mulib-ast.md) |
| Complete example | [`examples/modules/evtstream.c`](../examples/modules/evtstream.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/evtstream.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
