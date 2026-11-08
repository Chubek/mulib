# Ordered first-order term rewriting

[Manuals](README.md) · [Library index](index.md)

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

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-term_rewriting.h`](../include/mulib-term_rewriting.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-term_rewriting.h
 * @brief Bounded ordered first-order term rewriting on shared MuLIB ASTs.
 * Define MULIB_IMPL_TERM_REWRITING, MULIB_IMPL_AST and MULIB_IMPL_STRBUF.
 */
#ifndef MULIB_TERM_REWRITING_H
#define MULIB_TERM_REWRITING_H
#include "mulib-ast.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_term_rule { const mulib_ast_t *lhs, *rhs; } mulib_term_rule_t;
/** Leftmost outermost, rule-order rewriting. ?name leaf productions in patterns
 * bind subtrees; repeated names require structural equality. At most 32 distinct
 * variables per rule, RHS variables must occur on LHS. Productions and text match
 * exactly; action/offset metadata is ignored. Input/rules are borrowed proper AST
 * roots (depth<=128, <=65536 nodes each), as returned by mulib_tree_parse.
 * Return 1 normal form, 2 step limit, 0 invalid/OOM/resource limit. On 1/2, output
 * receives a new owned tree and optional steps; on 0 outputs remain unchanged.
 * Destroy output with mulib_ast_destroy. No termination/confluence guarantee.
 */
int mulib_term_rewrite(const mulib_ast_t *input, const mulib_term_rule_t *rules,
                       size_t rule_count, size_t step_limit, mulib_ast_t **output, size_t *steps);
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
| Header | [`include/mulib-term_rewriting.h`](../include/mulib-term_rewriting.h) |
| Implementation switch | `MULIB_IMPL_TERM_REWRITING` |
| Repository-local includes | [mulib-ast](mulib-ast.md) |
| Complete example | [`examples/modules/term_rewriting.c`](../examples/modules/term_rewriting.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/term_rewriting.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
