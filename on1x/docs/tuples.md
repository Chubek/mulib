# Tuples and local destructuring manual

## Syntax and typing

```on1x
fn swap(pair) -> _ {
  let (first, second) = pair;
  return (second, first);
}
fn main() {
  let (flag, answer): (bool, i64) = swap((42, true));
  assert(flag); println(answer);
}
```

Tuple expressions/types use comma-separated parentheses with at least two
elements. They are structural ordered products: every element type and arity
must match. Tuples may contain scalars, functions or other tuples. Ordinary
functions/lambdas can accept/return tuples and immutable tuples may be captured.
`_` holes work inside tuple annotations. Monomorphic inference includes tuple
constructors, pattern shape constraints, the occurs check and existing
node/work/depth/expanded-size budgets. Named nonexpansive tuple values may
generalize under the [scheme rules](polymorphism.md). Immutable pattern leaves
also generalize when their initializer is nonexpansive; ordinary parameter
patterns are monomorphic per global/lambda instantiation unless their fields
have explicit universal annotations.

`let (a, (b, c)) = value;` is an irrefutable local binding pattern. The initializer
evaluates once before any new names are defined. Each name becomes an immutable
local, and duplicates/current-scope redefinitions are errors. `_` is an ordinary
name, **not** a wildcard. Add an optional annotation for the entire pattern:
`let (number, flag): (_, bool) = value;`. Individual pattern-leaf annotations are
not supported. Ordinary function and lambda parameters also accept these
patterns: `fn sum((a, b)) -> _ { return a + b; }`. A pattern is one parameter,
not multiple arguments. Type inference constrains its tuple shape. All leaves
share the function body scope with other parameters, so duplicates across
patterns/plain parameters or body declarations are errors. Pattern leaves are
immutable and may be captured by escaping closures.

Parentheses without a comma group an expression/type: `(x)` / `(i64)` are not
tuples. A two-element tuple passed to a one-parameter function needs the extra
parentheses: `function((a, b))`; `function(a, b)` still passes two arguments.
There are no unit/empty or singleton tuples, trailing commas,
mutable destructuring, tuple assignment patterns, field/index projections or
refutable/exhaustive matching in this increment. Extern tuple patterns explicitly
flatten concrete scalar leaves into ordered C parameters; they do not declare
a C struct ABI. See [the flattening contract](polymorphism.md).
`var pair = (a, b); pair = ...;`
is allowed for whole-value replacement of the same monotype. Tuple fields
themselves cannot be mutated. Tuple equality, ordering, arithmetic and printing
are not defined; destructure and use scalar operations explicitly.

JSON owners/views and `void` cannot be elements, including inside nested tuples
or annotations. Function fields retain all existing signature/capture restrictions.
A mutable tuple binding cannot be captured; an explicit immutable snapshot may
be. String fields copy C-string pointers, not buffers; raw foreign string
lifetimes remain the caller's responsibility.

## C11 lowering and ownership

Every distinct tuple type becomes a dependency-ordered C11 record typedef with
`element_0`, `element_1`, ... fields. Type declarations are structurally
deduplicated and deterministically named, avoiding declared raw extern symbols.
Tuple elements evaluate left-to-right into temporaries before record construction,
so C initializer/argument evaluation order does not change language semantics.
Records are stored/passed/returned by value without a separate tuple allocation.

Scalar-only tuples need no drop operation. A tuple containing a function at any
depth owns references to the corresponding closure environments:

| Use | Reference behavior |
|---|---|
| Construct/copy local or capture field | Retains nested function environments |
| Function argument | Borrows fields for the call; caller keeps temporaries alive |
| Function result | Owns references, retained before the callee's lexical drops |
| Pattern binding | Each leaf independently retains its function/tuple fields |
| Whole-value assignment | Acquires replacement fields before releasing old fields |
| Temporary/local destruction | Releases nested environments in reverse field order |

Self-assignment is valid. Discarded tuples and condition/loop temporaries clean
up at the same boundaries as function temporaries, preserving previously
evaluated enclosing arguments/callees. Captured tuples retain their fields in
generated environment factories and release them in generated destructors.
The existing single-threaded environment runtime handles zero-count cascades
iteratively; tuples cannot bypass immutable/nonrecursive capture restrictions
to construct reference cycles.

Generated retain/release helpers take const typed record pointers, calling
child helpers rather than repeatedly expanding shared nested types. They are
emitted only when requested. Pattern lowering uses borrowed intermediate record
pointers, keeping access-path generation linear in pattern size. These pointers
never escape the binding statement, and retained leaf values remain live after
initializer cleanup. Pointer helpers do not alter the by-value language model.
Parameter patterns apply the same projection/copy operations in the function
entry prologue. The original parameter remains borrowed; extracted locals own
their references and participate in lexical/return cleanup. Compiler-internal
argument names cannot be spelled by source identifiers and are never emitted as
C identifiers. No implicit recursive bindings or foreign ABI changes result.

These compiler-owned records are **not** a foreign aggregate ABI. Raw tuple
results and tuple parameters without explicit flattening patterns are rejected;
flattened patterns pass only scalar C arguments. Exact reviewed aggregate
adapters require a future FFI implementation. No
object/function-pointer casts, third-party product runtime, new allocator or
runtime API are required. The closure runtime ABI remains version 1.

## Bounds and verification

The existing 2 MiB source / 256-level parser and expression bounds cover tuple
types, values and binding patterns. Inference additionally bounds expanded type
size and depth, including the outer declaration signature; sharing a tuple type
graph cannot evade these limits. Limits concern compiler input, not a guarantee
that arbitrary runtime stack allocations or recursion will succeed.

`on1x.types` checks tuple shape/order/arity, cached ownership/hole metadata and
API guards. `on1x.tuple_hir` checks pattern-driven inference, captured types,
occurs checks, transactional failures and idempotent checking/emission.
`on1x.tuples` compiles native strict C11 programs with counting environment hooks,
tests semantic/syntax failures separately, and checks evaluation order, escaping
captures, copy/assignment/return cleanup, generated names, finite shared type
graphs and bounded C output. `on1x.limits` checks deep and inclusive parser
boundaries. Real `on1x.json` tests also return scalar tuples after lexical JSON
cleanup. See [verification](../VERIFICATION.md) for executed configurations and
`examples/tuples.on1x` for a runnable higher-order example.
