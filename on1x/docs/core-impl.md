# Intrinsic nominal implementation blocks

Intrinsic `impl` blocks attach statically dispatched functions and values to a
nominal `record` or `enum`. They are part of the canonical parser, HIR, type
inference and C11 backend; no PPX or runtime registry is involved.

```on1x
record Box<T> { value: T }

impl<T> Box<T> {
  fn make(value: T) -> Box<T> {
    return record<Box<T>> { value: value };
  }
  fn get(self) -> T { return self["value"]; }
  fn choose<U>(left: T, right: U) -> U { return right; }
  value answer: i64 = 42;
  value item(self): T = self["value"];
}

fn main() {
  let box = Box<i64>::make(7);
  println(box::get());
  println(box::item);
  println(Box<i64>::answer);
}
```

`fn method(self, ...)` and `value property(self)` are receiver members.
Functions and values without `self` are associated members. A receiver is
lowered to the first ordinary function argument.

## Lookup and evaluation

`::` is a distinct inherent-member namespace:

- `value["field"]` projects a record field.
- `value::member` selects a receiver member from the value's canonical nominal
  type.
- `Type::member` and `Type<arguments>::member` select an associated member.
- Cliche lookup, such as `Show<T>::show(value)`, remains a separate namespace.
  A spelling that is simultaneously an imported cliche and nominal type is
  rejected as ambiguous.

Member lookup is deterministic and compile-time-only. Each declaration lowers
to a private ordinary function and then uses the existing inference,
specialization and C11 call paths. There is no runtime member table, string
lookup, reflection, virtual dispatch, or implementation-name resolution.
Member bodies and value initializers participate in the ordinary bounded,
transactional hygienic macro-expansion pass before checking and lowering.

Receiver evaluation becomes the first call argument and therefore remains
exactly once and left-to-right with the explicit arguments. Associated values
lower to nullary calls, and receiver values lower to one-argument calls; values
are evaluated on each access and are not memoized globals.

Associated functions may be stored as first-class function values. Receiver
methods currently require a direct invocation so the compiler can preserve the
receiver place/value semantics. Member type parameters are inferred from call
arguments; explicit `Type::member<...>` arguments are not admitted in this
profile.

## Generics, identity and coherence

Implementation parameters are ordinary explicit type binders:

```on1x
record Pair<A,B> { first: A, second: B }

impl<A,B> Pair<B,A> {
  fn flip(self) -> (A,B) {
    return (self["second"],self["first"]);
  }
}
```

Every implementation parameter must be determined by the target. Member
parameters follow the implementation parameters and use the existing bounded
monomorphization path.

The implementation must be in the same source module as the nominal
declaration. This orphan rule makes the member set part of one canonical type
identity. Imports, aliases and re-exports retain that identity; equal layouts
declared in different modules remain different types with independent member
sets.

A `(canonical nominal identity, member name)` may be declared only once.
Duplicate names reject even if two blocks use different concrete target
arguments, so source/import order cannot select an implementation. There is no
overloading, blanket-versus-exact preference, or extension implementation
mechanism. Use a core [cliche](core-cliche.md) when constrained generic
selection is required.

There is currently no `public`/`private` member modifier. A block does not place
its member names in the ordinary value namespace. Associated access requires a
visible nominal type spelling; receiver access is available when inference has
established the canonical nominal type, including values returned through an
imported API.

Field labels and member names may coincide because their access forms are
different. A local value shadows static type lookup, so `Box::item` uses a local
`Box` as a receiver when one is in lexical scope.

## Receiver ownership

`self` is a by-value receiver. It is admitted only for layouts recursively made
from copy-safe scalars and copy-safe tuples, records, enums and fixed arrays.
The declaration check is conservative for generic parameters, and every
concrete use is checked again after inference.

Strings, S-expressions, buffers, slices, JSON owners/views, functions, code,
`own<...>`, foreign resources and channel/fiber/other handles are not copy-safe
receiver fields in this profile. This restriction prevents a method call from
inventing an unmodeled retain, borrow, move or drop.

`borrow self` takes a shared, call-scoped loan when invoked on a named local,
an incoming borrow, or a named `own<Named>` (which lends its payload). It
uses the same lexical-place checks and C11 lowering as a direct call to
`fn inspect(value: borrow Named)`. It does not accept a temporary receiver,
retain a loan in a callback, or move a field out of the receiver. This allows
inspecting nominal values with immutable text or closure fields without
inventing an implicit by-value copy. Member properties may also use
`value item(borrow self): T`.

`ref self` and `move self` remain unsupported and produce
`ON1X-IMPL-RECEIVER`. Arbitrary borrow modes on other member parameters are
still unsupported.

## Backend and metaprogramming boundaries

Fully elaborated members use the direct C11 backend. A scalar-only associated
member can also use scalar MIR because it has become an ordinary scalar
function. Borrowed and aggregate receiver methods still receive the existing located
`ON1X-MIR-UNSUPPORTED` rejection; no aggregate is silently narrowed or
interpreted.

Implementation declarations are host-retained metadata. Unit-v1 PPX and
`.on1xi` interfaces cannot transport them and reject with
`ON1X-IMPL-UNSUPPORTED` rather than dropping a block. The restricted KernelBind
C11 profile also rejects them. Legacy `@enable Protocols` Category declarations
and intrinsic cliches keep their existing syntax, selection rules and trust
boundaries.

Current finite limits include 1,024 blocks per parsed unit, 256 members per
block, 4,096 loaded members, 32 combined implementation/member type parameters,
64 function parameters, bounded type validation and bounded generated AST and
specialization work. Limit exhaustion is distinct from duplicate, coherence,
missing-member, receiver and unsupported-profile diagnostics.

See [`examples/core_impl.on1x`](../examples/core_impl.on1x) and
`tests/test_core_impl.{cpp,py}` for generic records, enum receivers, module
identity, evaluation order, malformed HIR and backend-boundary coverage.
