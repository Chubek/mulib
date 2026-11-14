# Multi-stage programming (MSP)

On1x MSP uses **typed closed-code values with AOT lowering**, as selected for
this milestone. It is not runtime native compilation, an interpreter, or PPX.

```on1x
fn add(a: code<i64>, b: code<i64>) -> code<i64> {
  return quote { splice(a) + splice(b) };
}
fn main() {
  let x = 40;
  let c = add(quote { lift(x) }, quote { 2 });
  println(run(c));
}
```

## Semantics

| Form | Meaning |
|---|---|
| `code<T>` | Opaque code producing exactly `T`; distinct from `fn() -> T` |
| `quote { e }` | Construct a code value; defer `e` until execution |
| `splice(c)` | Inside a quote, evaluate `c: code<T>` one stage earlier and insert its execution |
| `lift(v)` | Inside a quote, evaluate `v: T` one stage earlier and embed the sampled value |
| `run(c)` | Execute `c: code<T>` and return `T` |

Splice/lift operands execute **once per construction**, in lexical traversal
order (callee, then arguments, then lambda bodies), even inside a quoted lambda
or short-circuited branch. Quoted effects execute on every run. Splicing samples
the code value, not its result: its effects repeat when the resulting code runs.
Lifting samples the value immediately. Capturing a mutable local through lift
is an explicit snapshot; lifting a managed buffer samples its reference, **not**
a deep immutable copy. This does not add effect or foreign-lifetime inference.

Code values are first-class, can be returned, passed, stored in tuples and
reassigned without changing type. Nested quotes produce nested code types.
Each splice/lift crosses exactly one quotation boundary; for example,
`quote { quote { lift(lift(x)) } }` samples an outer local correctly. A quote
contains one expression. Quote an immediately invoked lambda when a deferred
statement sequence is needed. `code<void>` is supported.

`run` is contextual: an ordinary local or global binding named `run` takes
precedence. `run!` remains an ordinary macro name. `quote`, `splice` and `lift`
are reserved. `code<code<i64>>=...` works without whitespace before `=`.

## Stage and type checks

Locals belong to a lexical quotation world, not just a numeric nesting depth.
An earlier local must enter quoted code via lift/splice. An inner quoted binding
cannot escape into construction-time operands, even by re-entering another quote
at the same numeric depth. This rejects scope extrusion such as:

```on1x
fn bad() {
  let c = quote { fn(x: i64) -> i64 { return splice(quote { x }); } };
}
```

Module-level functions/intrinsics remain available at every stage. Ordinary
lexical resolution, immutable-capture rules and type checks still apply. Raw
extern declarations cannot accept/return code, buffers or S-expression values;
no foreign layout or callback contract is inferred. JSON owners/views cannot
escape through code. Splicing an ordinary value, running a function instead of
code, or calling a code value as a function is a type error.

Nonexpansive quotations without splice/lift operands can generalize under the
existing immutable value restriction. Effectful construction is not generalized.
`code<T>` participates in unification, occurs checks, type-size limits and
explicit universal proof/instance matching; it is not an erased dynamic value.

## Lowering, ownership and limits

After PPX, `src/staging.cpp` checks stages and lowers each quote to a factory
and a zero-argument closure. Factory arguments evaluate construction-time
operands; the returned closure owns their immutable captures. Splices become
typed code invocations in that closure. Nested factories preserve stage timing.
Type inference and the ordinary checker validate the resulting HIR.
Stage lowering/inference commit transactionally from a cloned unit.

The backend reuses exact typed closure structs and the existing reference-counted
environment ABI (version 1), without function-pointer casts. Code copies, tuple
fields, return values, assignment and temporaries follow the closure ownership
rules. Bodies are precompiled C11; dynamic code composition selects/composes
those bodies. There is no runtime AST, source eval, JIT or compiler execution.
Native programs need only their generated C11 and `runtime/on1x_runtime.c`.

Limits include 64 quotation stages, 262,144 lowered AST nodes, 256 lowered
expression levels, and the existing inference, specialization and capture
budgets. Lowered depth can reject an input before the stage-count bound.
Runtime recursion/stack exhaustion is not statically proved absent. Fatal
arithmetic/allocation errors do not promise unwinding; references are single-threaded.

Run `on1x.staging` and `on1x.meta_hir` for effect timing, nesting, polymorphic
code, scope extrusion, ownership/leak counters, wire round trips, deterministic
emission, limits and transactional failure tests.
