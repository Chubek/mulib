# mulib-walker

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
