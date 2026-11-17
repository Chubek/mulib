# M1 nonrecursive nominal data

This task implements section 11's initial, nonrecursive profile. It does not
implement owning lists, boxed recursion, managed data fields, defer, typed error
propagation or concurrency.

```on1x
record Point { x: i64, y: i64 }
enum Option<T> { None, Some(T) }
enum Result<T,E> { Ok(T), Err(E) }

fn main() {
  let p = record<Point>{y: 2, x: 40};
  let answer = variant<Option<i64>>(Some, p["x"] + p["y"]);
  println(match answer with { None() => 0, Some(n) => n });
}
```

## Selected semantics

- Declarations are intrinsic, module-level, immutable products/tagged sums.
  Existing structural data syntax and semantics remain unchanged. Constructors
  use explicit `record<Name<args>>{fields}` and `variant<Name<args>>(Tag,payload)`
  spellings. Empty payloads use `record{}`; multiple payloads use a tuple.
  Constructor tags are selected from that named enum's catalog, not global
  functions. Patterns use the existing constructor patterns on a known enum.
- Identity is the declaring module plus declaration, with invariant type
  arguments, including unused (phantom) arguments. Re-export/alias references
  preserve identity. Equal representation never implies assignability.
- Parameters bind in declaration fields only; arguments are checked for arity.
  `_` arguments participate in existing inference/generalization. `forall`
  bounds and specialization retain nominal identity and argument sharing.
- Only the existing bounded trivial by-value field profile is admitted. There
  are no nominal heap allocations, unique moves, destructors, borrowed fields or
  new effects.
  Construction evaluates operands once, left-to-right, before ordinary lexical
  cleanup. Aggregate projection/pattern inspection does not erase identity.
- Declarations are validated even if unused. Direct/mutual recursion, unknown
  types, expansive layouts, resource/managed fields and higher-kinded/recursive
  applications fail with located diagnostics. Depth, work and declaration
  budgets apply before layout expansion; existing value storage limits remain.
- C11 emits distinct internal structs for distinct nominal instantiations.
  These are whole-unit language types, not an exported/separate-compilation or
  foreign C ABI. MIR/SDK profiles without aggregate models reject; no scalar
  approximation or runtime ABI change is introduced.
- PPX transports declarations and named *references*, never nominal layout
  certificates. Post-transform inference constructs layouts from declarations;
  the ordinary checker independently validates them against that catalog.

Validation results and unsupported profiles are recorded in `../VERIFICATION.md`.

```sh
python3 on1x/tools/on1x-build.py on1x/examples/nominal_adts.on1x \
  --on1xc build/on1x/on1xc -o build/nominal-adts
build/nominal-adts
ctest --test-dir build/on1x -R 'on1x.nominal' --output-on-failure
```

The example prints `42`, `true`, `-7`. It uses explicitly matched `Result`
values, not an implicit error propagation/unwinding mechanism.

## Bounds, ownership and migration

The ceilings are 512 declarations per source module, 4,096 in a loaded unit,
32 parameters/arguments, 256 fields/constructors/payload components, 32 layout
levels and 4,096 expanded storage units. Catalog validation/expansion is bounded
at 262,144 work units. Cached layouts retain their **height**, so visiting a
deep dependency after a shallow cache hit cannot bypass the depth limit.
Checked-HIR verification has a 262,144-node/520-level combined traversal budget,
preserving the existing parser's separate statement/expression depth limits.

| Value profile | Copy / move / drop | Thread transfer |
|---|---|---|
| Admitted nominal products/sums | Trivial by-value copy/move; no destructor | Not a checked concurrency capability |
| Closure capturing an immutable nominal value | Existing RC environment owns its snapshot | Existing single-thread RC restrictions |
| JSON owners/views, shared buffers, closures or text in nominal fields | Rejected | Rejected |

These additions do not alter ordinary structural type equality. Named types
occupy the loader's type namespace, not the value namespace; local variables do
not capture macro-definition-site type references. Named/selective/relative
imports and re-exports retain the declaring module's identity. Legacy quoted
imports keep their existing flat conflict policy. There is no automatically
imported `Option`/`Result`, bare constructor function, boxing or field mutation.
Declare/import types explicitly; existing tuple/status/sexpr APIs are unchanged.

PPX v1 adds `NamedType` and `NominalDeclaration`. The unchanged `Unit` envelope's
declaration list now accepts the `Declaration` superclass of functions and named
data declarations. Rebuild schema-pinned native processors. Unknown constructors
must reject, not be treated as functions; existing derive/cfg/FFI processors skip
data declarations. Signature derives remain syntactic, not ADT proof/deriving.

`record` and `variant` remain contextual in expressions: variables with those
spellings can still appear in ordinary comparisons. Typed-constructor parsing
requires a complete `<type>` prefix followed by `{` or `(` respectively.
