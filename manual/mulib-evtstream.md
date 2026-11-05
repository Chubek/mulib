# mulib-evtstream

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
