# Ordered first-order term rewriting

Define `MULIB_IMPL_TERM_REWRITING`, `MULIB_IMPL_AST`, `MULIB_IMPL_STRBUF`;
also `MULIB_IMPL_TREE_PARSING` if parsing term strings.
[Example](../examples/modules/term_rewriting.c).

Rules borrow lhs/rhs AST roots. Pattern `?name` leaf productions bind subtrees;
repeated names require structural equality. Up to 32 variables per rule; RHS
variables must be bound by LHS. Production/text bytes match exactly, with ordered
children; offsets/actions are ignored and not retained in output. Proper owned
AST root topology, depth<=128 and <=65536 nodes per tree are required. Reserved
`?` productions must be valid variable leaves, including in the input.

The engine clones input, then repeatedly applies the first matching rule at the
leftmost outermost node, restarting at root. Replacement is copied before old
nodes are destroyed. Input and rules never change. Return 1 normal form, 2 step
limit with an owned partial result, 0 invalid/OOM/resource limit with outputs
unchanged. `step_limit=0` still checks normal form. Destroy successful result
with `mulib_ast_destroy`; output must not already own a live tree.

Depth/node limits also apply after replacement, and clone construction is bounded.
There is no confluence/termination/semantic-equivalence guarantee, conditional
matching, arithmetic evaluation, unification or equality saturation. Runtime
depends on tree/rule size and step_limit; use small trusted limits for untrusted
terms. This complements, rather than replaces, the egraph rewrite module.
