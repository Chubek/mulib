# Intrinsic core cliches

Core cliches are On1x's intrinsic, coherently selected interface mechanism. They
are parsed, type-checked, resolved, and lowered by the canonical compiler:

```on1x
record Box<T> { value: T }

cliche Show {
  fn show(value: Self) -> string;
  let fallback: Self;
}

impl Show for i64 {
  fn show(value: i64) -> string { return format("{}", value); }
  let fallback: i64 = 0;
}

impl<T> Show for Box<T> where Show<T> {
  fn show(value: Box<T>) -> string {
    return Show<T>::show(value["value"]);
  }
  let fallback: Box<T> =
    record<Box<T>> { value: Show<T>::fallback };
}

fn render<T>(value: T) -> string where Show<T> {
  return Show<T>::show(value);
}
```

`Self` is implicit cliche parameter zero. Named cliche parameters follow it.
Function members and associated values may use `Self`, the cliche parameters,
and their own explicit type parameters. Bounds can use either
`T: Cliche` shorthand or a `where Cliche<T, ...>` clause.

## Lookup and lowering

Lookup is always explicit:

```on1x
Show<i64>::show(42)
Show<i64>::fallback
```

The first type argument is `Self`; any declared cliche arguments follow it.
Member type arguments, when present, follow the member name, for example
`Convert<i64>::to<string>(value)`. Associated values have no argument list.
`Type::member` inherent lookup is a separate HIR path and does not participate
in cliche selection.

Resolution occurs at compile time. Each selected member becomes an ordinary
private compiler-generated function with a deterministic name. Associated
values become calls to generated nullary functions. Emitted applications have
no implementation registry, string lookup, reflection, or compiler dependency.
A projection may survive only inside a non-emitted constrained generic
prototype; every executable specialization must resolve before semantic or
backend admission.

## Coherence and bounds

An implementation must provide every declared member exactly once and with the
substituted declaration signature and member bounds. Every implementation type
parameter must occur in its head.

- Two exact heads are rejected.
- Overlapping generic heads are rejected conservatively.
- One exact implementation may coexist with and overrides one matching generic
  implementation.
- An implementation is allowed only in the source module that defines the
  cliche or the nominal `Self` type. Scalar implementations therefore belong
  with the cliche declaration.
- Generic prerequisites must decrease structurally. Resolution also has
  independent work and depth budgets and detects an obligation already on the
  active stack.

Diagnostics keep incoherence, orphan implementations, unsatisfied obligations,
ambiguous selection, recursive obligations, malformed signatures, and resource
limit exhaustion distinct. A generic projection is valid only when its exact
predicate appears in the active function or implementation assumptions.

Declarations, implementations, constraints, and qualified projections retain
canonical module identities through normal named, selective, relative, and
quoted imports. Module aliases do not create a second cliche identity.

## Boundary with legacy Category and PPX

Core `cliche`/`impl` is not the legacy
[Category extension](category.md). Category's `protocol`/`impl` carriers,
processor registry, evidence plans, and trust boundary are unchanged. Unit-v1
PPX/schema transport cannot represent core cliche declarations and rejects
rather than dropping them. A source graph cannot combine intrinsic cliche
declarations with legacy declaration extensions or source PPX processing.

Current cliches are first-order declaration contracts with statically selected
functions and associated values. Associated *types*, specialization beyond the
exact-over-blanket rule, negative implementations, dynamic dictionaries,
runtime reflection, separate compiled interface transport, and recursive
nominal layouts are not supported. Fully elaborated programs use the existing
C11 backend; the scalar MIR still rejects any operation or type outside its
documented profile.
