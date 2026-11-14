# Polymorphism, specialization and flattened extern patterns

## Value-restricted lets and destructuring

```on1x
fn main() {
  let id = fn(x) -> _ { return x; };
  let copy = id;
  println(id(42)); println(id(true)); println(copy("text"));
}
```

Named immutable `let` bindings generalize free inference variables in
nonexpansive values: literals, names, lambdas, and tuples of nonexpansive values.
Each use gets a fresh instance. Generalization excludes variables free in the
lexical environment, free variables of global schemes or the enclosing return type.
Aliases preserve generalization; type annotations constrain the scheme.

`var` bindings and expansive initializers (calls, operators, etc.) are
monomorphic. Immutable destructuring generalizes eligible variables independently
for each leaf, including nested patterns and aliases of nonexpansive tuple
values. Concrete siblings still receive ordinary runtime bindings; shared weak
variables stay shared. Initializers must still satisfy the value restriction.

```on1x
fn main() {
  let (id, flag) = (fn(x) -> _ { return x; }, true);
  println(id(42)); println(id(flag));
}
```

There are no recursive local lets. Omitted return annotations still mean `void`,
not inferred returns. `_` remains an ordinary binding name, not a wildcard.

Overloaded intrinsic/equality operand variables are weak, not generalized.
They need concrete constraints from annotations or uses; no default type or
type-class instance is guessed. Captured weak variables remain shared across
specializations. JSON/void parameter restrictions, immutable/resource capture
rules and ordinary definite-return checks still apply.

Generic definitions, including unused ones, are type-checked with quantified
type identities. Unused invalid bodies are errors, not dead code silently
discarded before checking. Resource-bearing specializations are rejected by
the ordinary checker, just like handwritten monomorphic functions.

## Global functions and recursion

Ordinary global function declarations, including imports, generalize after
their dependencies have been inferred. A lexical-name-aware traversal discovers
strongly connected recursive components. Each component is inferred with shared
monotypes; eligible variables then generalize at its boundary. Calls from outside
the component instantiate fresh signatures. Recursive calls within one instance
share its monotypes: **polymorphic recursion is not inferred**.

```on1x
fn repeat(x, count: i64) -> _ {
  if count == 0 { return x; }
  return repeat(x, count - 1);
}
fn main() { println(repeat(42, 2)); println(repeat(true, 3)); }
```

Mutually recursive components specialize together. Local shadowing takes
precedence over global dependencies. `main` and raw externs never generalize.
There is no new top-level `let` syntax; globals here means function declarations.
Weak overloaded variables still require concrete constraints from annotations
or uses rather than generalizing or defaulting.

## Explicit universal parameters

Ordinary function/lambda parameters have one monotype **per instantiation**.
To use the same parameter at different types within one body, annotate it:

```on1x
fn use(id: forall(a) fn(a) -> a) {
  println(id(42)); println(id(true));
}
fn main() { use(fn(x) -> _ { return x; }); }
```

`forall(a, b) fn(a, b) -> a` binds distinct type variables; binder spelling is
alpha-equivalent. The body must be a fully specified function type (no `_`
holes or nested `forall` binders). Quantified variables are scoped only over
that type, not lambda bodies or neighboring annotations. Instantiation is
predicative: a bound variable cannot be replaced by a type containing `forall`.
Resource/void parameter, tuple and foreign-signature restrictions still apply.

Universal parameters can be forwarded, aliased, captured by immutable closures,
returned with explicit universal result annotations, or stored/destructured in
tuples. Fully explicit function types may contain universal parameters. No
higher-rank type is guessed for an unannotated parameter: `fn use(f) { f(1);
f(true); }` is still rejected; add `forall` to request universal behavior.

Introducing a universal value checks a nonexpansive initializer against fresh
**rigid** type variables. It must work for arbitrary distinct variables without
specializing any monomorphic environment. This check also runs for unused
arguments. An `i64` identity, a call result, a weak mutable function binding,
or a lambda performing scalar-only operations on a quantified value cannot
masquerade as `forall(a) fn(a) -> a`. Existing universal values can be copied
and passed normally. `var` still does not infer/generalize a scheme; an explicit
universal annotation may hold/replace already universally checked values.

## C11 elaboration and limits

The compiler retains checked generic prototypes in HIR and creates concrete
bindings at the original declaration point for local static uses. Global
components produce concrete sibling function definitions. Fresh inference
variables do not escape as an erased runtime value. Specialized bindings and
their typed invocation/environment records use the existing C11 backend and
reference-counting contract. Local captures are sampled at the original declaration
point, not at a later read. Allocations and number of specialized environments
are implementation details; there is no function identity/equality operation.

Unused prototypes and prototype-only instances are not emitted. Concrete
instances must be fully grounded; ambiguous instantiations are errors rather
than arbitrary defaults. Nested schemes and aliases are elaborated recursively.
The entire inference/elaboration pass works on a clone and commits only on
success, so failed inference/specialization does not partially rewrite a unit.

In addition to existing inference budgets, defaults cap specialization at 1,024
static instances, 64 nested specialization steps, 512 schemes, 8 MiB of scheme
environment accounting (name bytes plus 64 bytes per entry), and 262,144 cloned
expression/statement nodes per cloning phase. Exceeding a budget produces a located error. This
is bounded specialization, not a claim of unrestricted Hindley–Milner or effect
inference. Global dependency inference has a 128-frame limit. `--types` lists
public global signatures with normalized `forall(a0, ...)` binders and hides
synthetic specialization names; it does not list local schemes.

Universal runtime values use an internal C11 record of exactly typed closure
instances, one for each concrete demand for that closed universal type in the
unit. All packs of alpha-equivalent types share the same deterministic layout;
projections evaluate their input once. The demand pass iterates to a fixed point
(at most 128 rounds), using the shared specialization/node/work/clone budgets.
Unbounded or excessive demand is a compile-time error, not erased unsafe calls.
An unused universal value has a dummy-byte record, not an empty C struct.
Rigid proof bodies and generic-only projections are checked but never emitted.

The bundle owns its closure fields; copying, argument borrowing, returning,
tuple extraction, capture and destruction reuse recursive retain/release
helpers. No `void *` value erasure, function-pointer casts, runtime type tags or
foreign callback ABI are introduced. Bundle layout is an internal whole-unit
ABI, not a separately linkable universal ABI. Packing a nonexpansive value may
allocate multiple environments, which is an implementation detail. Allocation
failure terminates through the existing runtime rather than unwinding.

## Explicit flattened extern patterns

```on1x
extern fn raw((number, (enabled, text)): (i64, (bool, string)), extra: i64) -> i64;
```

This explicitly declares the raw C ABI:

```c
int64_t raw(int64_t number, bool enabled, const char *text, int64_t extra);
```

The On1x call takes two logical arguments: `raw((40, (true, "ok")), 2)`.
Patterns flatten depth-first, left-to-right. Every pattern node must match its
concrete tuple annotation; a leaf must be `i64`, `bool` or `string`. Duplicate
leaf/plain-parameter names are errors. Leaves cannot be tuples, functions, JSON
owners/views, `void` or type holes. Nested tuples require nested patterns.
Tuple parameters without patterns and tuple-valued results remain unsupported.

Arguments evaluate once, in the existing callee-first/left-to-right order,
before their scalar fields are passed to C. No C struct layout, aggregate
calling convention, ownership transfer, callback contract or safe-FFI proof is
inferred. The user must supply an exactly compatible C function; this is still
unsafe raw interop, and foreign string lifetimes remain the caller's obligation.
Extern names remain direct-call-only and cannot become generic function values.

See `examples/polymorphism.on1x`, `examples/global_polymorphism.on1x`,
`examples/destructuring_polymorphism.on1x`, `examples/polymorphic_parameters.on1x`,
`tests/fixtures/extern_patterns.c`, and registered `on1x.poly` /
`on1x.poly_extended` tests. Existing mutable-lambda and foreign rejection tests
now specifically exercise the value restriction / unsupported leaf cases rather
than rejecting these newly admitted features.
