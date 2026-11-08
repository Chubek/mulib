# mulib-peephole

[Manuals](README.md) · [Library index](index.md)

Guarded shallow-tree rewriting for native 64-bit wrapping operations.

Define `MULIB_IMPL_MIR`, `MULIB_IMPL_LOWOPT`, and `MULIB_IMPL_PEEPHOLE` in one C11
translation unit before including `mulib-peephole.h`. Link `-lm` for MIR.

`mulib_peephole_rules(&count)` exposes a static, immutable catalogue of **75
rewrite rules**. `mulib_peephole_apply` scans a caller-supplied catalogue in order,
checks root opcode and guard, and applies the first matching replacement. It
allocates nothing and returns 1 (rewritten), 0 (no match), or -1 (invalid).
Output and optional matched-rule index remain unchanged when no rule applies.
`mulib_peephole_validate` checks every catalogue entry, including guard/action
compatibility, operand arity and pure rewrite roots. `apply` also validates the
input instruction shape, known-operand mask and nested-definition references
before changing anything; invalid input preserves both outputs.

Rules match constants, repeated register operands, branches, masked shift counts
and selected two-level binary trees. Replacements include constants, copies,
doubling, branch elimination and algebraic cancellation. See the complete
catalogue in [`library/optim`](../library/optim/README.md).

## Facts and legality

The caller supplies `mulib_peephole_facts_t`: proven operand constants and optional
left/right reaching definitions. A nested definition is usable only when all of
its operand registers still hold their values from that definition. For example,
`(x+y)-y` can become `x` only if the original `x` and `y` are still available.
The OptML driver proves this with block-local reaching definitions and write
invalidation, including self-updates. Shared definitions are not implicitly
deleted. NULL nested pointers disable tree rules.

All built-in rules use native MIR's **wrapping 64-bit arithmetic** and shifts
masked to six bits; they cannot be applied directly to checked arithmetic or
floating-point IR. Memory operations and calls have no built-in rewrite rules.
OptML enforces KEEP/VOLATILE/BARRIER annotations before invoking this matcher.
The standalone matcher trusts facts and custom rule equivalence; it is not an
SMT prover or a whole-program validator.

`tests/test_isel.c` exercises every rule separately against an independent
arithmetic evaluator, including random words and each rule's boundary literals.

```sh
cc -std=c11 -Iinclude examples/modules/peephole.c -lm -o peephole
./peephole
```

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-peephole.h`](../include/mulib-peephole.h) |
| Implementation switch | `MULIB_IMPL_PEEPHOLE` |
| Repository-local includes | [mulib-lowopt](mulib-lowopt.md) |
| Complete example | [`examples/modules/peephole.c`](../examples/modules/peephole.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/peephole.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
