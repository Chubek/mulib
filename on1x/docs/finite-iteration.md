# Finite snapshot iteration

```on1x
let rows = array[(1,(10,99)),(2,(20,99)),(3,(30,99))];
total: for (index,(points,_)) in rows[1:3] {
  if index == 2 { continue total; }
  println(points);
}
```

`for pattern in operand { ... }` is a statement. The operand must have
a known fixed array or snapshot slice type (`array<T,N>` or `slice<T,N>`).
Annotate a parameter if its shape is otherwise unknown. An empty array
literal still needs enough ordinary element-type context.

The operand evaluates once, before any pattern names exist. The loop retains
a value snapshot, reads its logical length once, and visits indices in
increasing order. Replacing the source variable during the body does not
change that snapshot. A sliced value keeps its composed offset and runtime
length; backing capacity is not the number of iterations. An empty array or
slice still evaluates the operand and then skips the body. A direct `borrow`
expression is not an iteration operand; this feature does not introduce a
loan or an element reference. Reading an already-borrowed named array/slice
as a value takes a copy under the existing borrow rules.

Patterns are irrefutable: a name, `_`, or a possibly nested tuple of these
patterns. Every named leaf is a fresh immutable local, visible only in the
iteration body. `_` introduces no source-visible name. Duplicate names and
tuple type/arity mismatches reject even on an empty value. A block may shadow
a binder as usual. Historical `for name in lo..hi`/`lo..=hi` remains separate:
its single identifier (including `_`) is still a binding, not a wildcard.

`continue` takes the target latch, advancing one index after lexical cleanup.
`break` skips that latch. A labeled exit uses the same rules across nested
loops; `return` uses normal function cleanup. Existing fatal trap and
function-scoped `defer` semantics do not change.

## Lowering and backend limits

The parser expands finite iteration into existing source `Scope`, `Let`,
`ControlWhile`, index and length operations. Macro hygiene alpha-renames
internal names before Unit-v1 PPX transport. Inference and semantic checking
restrict the preheader length operation to arrays/slices and reject a borrowed
alias; successful checked HIR contains only the ordinary data operations and
a verified loop target/latch. No scanner tokens, ASDL constructors, MIR
opcodes or runtime ABI are added.

The direct C11 backend and the VMWeave product lowering execute this profile
subject to their existing type, layout, and body restrictions. VMWeave does
not inherit C11's foreign/resource/cleanup support merely by admitting this
loop. The ordinary scalar MIR evaluator, MIR-to-C11 route and the ordinary
source-to-Archon route do not admit the required array/slice layouts and do
not silently fall back. See [VMWeave](../backends/mir-vm/README.md).

Arrays and slices are the only admitted operands. The stdlib compatibility
lists use `sexpr`, and a user-defined recursive `own<List>` has no intrinsic
iteration semantics. Regex captures are not iterable either, even though
they support indexing separately. Intrinsic typed lists, cons/tail and list
patterns, constructor/record/array or guarded/refutable iteration patterns,
filtering/comprehensions, and general iterator/drop protocols are not
implemented by this feature.

Run `examples/finite_iteration.on1x` for `100`, `30`, and `9`, or use
`ctest --test-dir build/on1x -R '^on1x\.control(_hir)?$' --output-on-failure`
after building with tests. This checks direct C11, verified standalone VM
artifacts with and without optimization, source and HIR rejection, identity
PPX, staging, and C11 lifetime accounting.
