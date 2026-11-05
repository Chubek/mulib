# OptML optimization specifications

`default.optml` lists all **75 implemented guarded rewrite rules**, in execution
order. Load its bytes with `mulib_optml_parse`, then set `options.rules` and
`options.rule_count` from the resulting policy. Keep the policy alive throughout
the synchronous optimization call. The default options use the identical static
catalogue without opening files.

All implementation code, including the rule patterns, lives in
[`include/mulib-peephole.h`](../../include/mulib-peephole.h) and
[`include/mulib-optml.h`](../../include/mulib-optml.h). These are 75 transformations
executed by one shallow-tree matcher, plus a conservative dead-write pass.

## Rule families

| Family | Rules | Semantic guard |
|---|---:|---|
| Constant evaluation | 13 | Proven constant operands at this program point |
| Identity/absorbing literals | 17 | Zero, one, or all-one word |
| Shift simplification | 7 | Count masked to six bits; logical/arithmetic distinction |
| Repeated operands | 7 | Same current virtual-register value |
| Doubling | 3 | Multiply by two or shift left by one |
| Comparison bounds | 4 | Unsigned zero/max or signed min/max |
| Branches/self copy | 4 | Proven condition, equal successors, or self assignment |
| Two-level tree identities | 20 | Stable reaching definition and unchanged input registers |

All arithmetic wraps at 64 bits. Signed comparison is explicit; shifts mask the
count to six bits. Rules never fold memory accesses or calls. The two-level rules
cover absorption, idempotence, XOR cancellation and add/subtract cancellation.
Multi-use definitions are retained; the ordinary dead-write pass decides whether
they can subsequently be removed.

## Policy syntax

One case-sensitive rule name per line, optional surrounding ASCII whitespace and
`#` comments. Blank lines are ignored. Unknown names, duplicate rules and multiple
names on a line fail with a byte offset. An empty file disables all peephole
rules; `dead_writes` is an independent option. Policies choose/order trusted
transformations rather than accepting executable expressions.

For application-specific rules, pass a C array of `mulib_peephole_rule_t` with the
documented guards and actions. Applications must establish their equivalence.
`tests/test_isel.c` checks every built-in rule against an independent unsigned
arithmetic evaluator, including random words and extreme literals.

See [OptML](../../manual/mulib-optml.md),
[instruction selection](../../manual/mulib-isel.md), and
[superinstructions](../../manual/mulib-superinstr.md).
