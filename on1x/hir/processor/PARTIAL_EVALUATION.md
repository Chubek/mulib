# Typed partial evaluation

`--enable-opt partial-eval` specializes checked source before logical
desugaring. The `release` profile enables it; `default` and `debug` leave
it off. It is a compiler transform, not a compile-time execution facility.

```on1x
fn scale(k:i64,x:i64)->i64 { let factor=k*2+1; return x*factor; }
fn probe(x:i64)->i64 { return scale(4,mark(x)); }
```

The known parameter `k=4` determines `factor=9`. The residual body of `probe`
evaluates `mark(x)` once, then performs checked multiplication by `9`. With
`scale(4,5)` both parameters are static and the call becomes literal `45`.
This works without running MIR passes or an external optimizer.

## Admission and fallback

* Closed i64 and Boolean unary, arithmetic, comparison and logical operations
  can produce literals. Integer results use the MIR evaluator's checked
  arithmetic, including its signed division and remainder semantics. Overflow,
  invalid division and invalid remainder leave executable operations in place.
  Floating point, unsigned arithmetic and foreign operations are outside the
  evaluator's vocabulary.
* A direct named function must have checked, by-value scalar parameters, a
  scalar return, and a body consisting of a final return optionally preceded
  by immutable scalar `let` bindings. Every initializer must specialize to a
  known value. The interpreter reads the original checked definitions; it does
  not evaluate closures, foreign calls, mutations, I/O, borrows, loops, or
  arbitrary statements. Nested eligible calls can be evaluated on static
  arguments.
* A mixed call needs a static argument and exactly one dynamic argument.
  Specialization constructs a proposed residual tree with a hole, then checks
  that this argument will execute exactly once on every path. A parameter used
  twice, omitted, or hidden behind a short-circuit condition prevents
  specialization. Other subexpressions must have folded successfully, so none
  can trap before the original argument is evaluated. The accepted residual
  moves the original checked operand; its effects and source location stay
  intact. Remaining operators keep their callee locations for trap reporting.
* No functions are generated. Work is capped at 262,144 units per translation
  unit. One trial admits at most 16 arguments, 16 statements, eight active
  calls, 256 expression visits and depth 32. Cycles, operations outside the
  admitted subset, and size/depth growth beyond the allowed residual budget
  leave the original call in place. The source traversal depth is capped at
  128. A budget stop leaves a semantically equivalent residual program.
* Source typing, ownership checking and semantic constraints precede the
  transform; the compiler rechecks its private candidate before publication.
  The `partial_eval` object in `--dump-processor` reports `enabled`, `work`,
  `limited`, `folds`, `static_calls`, and `residual_calls`. The last three
  count committed changes, not speculative evaluations.

`tests/test_partial_eval.py` checks actual residual MIR and compares execution
and diagnostics with the pass disabled through direct C11 and MIR evaluation.
