# M1 nonrecursive nominal data

This document records section 11's initial, nonrecursive profile. That increment
did not implement owning lists, boxed recursion, managed data fields, defer,
typed error propagation or concurrency.

The later [shared-ownership profile](owning-adts.md) admits immutable `sexpr`
fields in nonrecursive nominal declarations. The still later
[finite unique-owner profile](recursive-owning-adts.md) admits recursive nominal
layouts through `own<Named>` with explicit moves and C11 cleanup.

The direct C11 profile also admits immutable `string` and checked `fn(...)`
values in nominal fields and type arguments. Text uses the existing literal
or immutable runtime-snapshot lifetime; storing text in a record does not
copy foreign bytes or extend a raw extern's storage lifetime. Closures use the
existing single-threaded environment retain/release contract, including for
nested nominal records and the active payload of nominal enums. Closure
signatures do not break nominal type cycles. Structural aggregates, mutable
buffer fields, JSON resources, foreign layouts, cross-thread closure transfer,
and serialization of closure environments are not admitted by this extension.

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
- In this initial profile, only the existing bounded trivial by-value field
  profile is admitted. It has no nominal heap allocations, unique moves,
  destructors, borrowed fields or new effects. Later ownership profiles are
  additive and do not change these copy semantics for non-owning values.
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
| Immutable text in nominal fields | Snapshot pointer copy; no per-value destructor | No added transfer capability |
| Checked closures in nominal fields | Existing environment retain/release | Single On1x runtime thread only |
| JSON owners/views or shared buffers in nominal fields | Rejected | Rejected |

These additions do not alter ordinary structural type equality. Named types
occupy the loader's type namespace, not the value namespace; local variables do
not capture macro-definition-site type references. Named/selective/relative
imports and re-exports retain the declaring module's identity. Legacy quoted
imports keep their existing flat conflict policy. There is no automatically
generated bare constructor syntax or field mutation. Boxing is available only
under the later explicit [unique-owner contract](recursive-owning-adts.md). The later
pervasive-library increment automatically imports canonical `Option`/`Result`
declarations and ordinary `option_*`/`result_*` constructor helpers. Local
declarations such as this document's example retain precedence and their
distinct nominal identity. `--no-prelude` still requires explicit declarations
or imports; existing tuple/status/S-expression APIs are unchanged.

PPX v1 adds `NamedType` and `NominalDeclaration`. The unchanged `Unit` envelope's
declaration list now accepts the `Declaration` superclass of functions and named
data declarations. Rebuild schema-pinned native processors. Unknown constructors
must reject, not be treated as functions; existing derive/cfg/FFI processors skip
data declarations. Signature derives remain syntactic, not ADT proof/deriving.

`record` and `variant` remain contextual in expressions: variables with those
spellings can still appear in ordinary comparisons. Typed-constructor parsing
requires a complete `<type>` prefix followed by `{` or `(` respectively.
