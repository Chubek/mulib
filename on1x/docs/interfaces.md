# Checked project interfaces (.on1xi)

A declared interface is a **source conformance check**, not a module export
list or a separately compiled artifact. In `On1xfile.i`, `interface "values.on1xi"`
pairs `values.on1xi` with a declared `source` or `library` named `values.on1x`
in the same directory and package. For a package initializer, the pair is
`__init__.on1xi` and `__init__.on1x`.

```on1x
# values.on1xi
on1xi 1;
fn make(label: string, count: i64) -> (string, (i64, bool));
fn print_count(count: i64) -> void;
```

```on1x
# values.on1x
fn make(name: string, n: i64) -> (string, (i64, bool)) {
    return (name, (n, true));
}

fn print_count(n: i64) { println(n); }
```

## Grammar and comparison

- The required version header is `on1xi 1;`. A file then has one or more
  `fn name(parameter: type, ...) -> type;` declarations. Layout and `#`
  comments follow ordinary On1x rules. Names must be unique within the file;
  parameter names must be unique within a declaration.
- V1 admits `i64`, `bool`, `string`, and nested tuples of these types.
  `void` is allowed only as the whole return type. Parameter order and
  exact structural type (including result, arity and tuple order and
  nesting) must agree. Parameter names do not have to match. Implementation
  parameter patterns are allowed with explicit types. The normal default
  of `void` for an omitted implementation return type still applies.
- Each member must correspond to a single local On1x function definition
  with a fully annotated signature: holes and types inferred in an
  implementation header do not satisfy the contract. Extern declarations
  and imported or re-exported names cannot stand in for a definition.
  The compiler also checks function bodies normally.
- Other shapes reject rather than degrading to an opaque annotation.
  These include function-valued, quantified, nominal, numeric types other
  than `i64`, records, sums, arrays, slices, code and resource/handle types.
  Interfaces cannot contain implementations, attributes, imports, macros,
  PPX directives or extensions. The verification mode does not execute
  PPX or declaration extensions and cannot be combined with KernelBind.
- The verifier uses normal scanner and type-parser budgets, plus at most
  256 interfaces per compiler invocation, 256 members per interface,
  32 parameters per signature and 32 elements per tuple. Each interface
  is subject to the ordinary 2 MiB source-file limit and counts toward
  the loader's aggregate source byte budget.

## Identity and project integration

The compiler binds each member to the definition owned by the canonical
same-directory, same-stem implementation path in the loaded source graph.
The loader translates that definition to the unit's internal name; the
checker verifies that exact definition and its checked type. An alias, file
of the same name in another search directory, or coincidentally equal
signature in another module is not a substitute. A type mismatch points at
the interface declaration and reports the expected and actual types and the
implementation location.

The [project driver](../stdlib/on1xfile/README.md) verifies all declared
pairs from the staged inputs, not the backing checkout. It uses the existing
dependency and project graph policies; dependency verification cannot use
undeclared application modules. It checks pairs in a separate front-end
invocation rooted at their implementation, even if no selected target
reaches that implementation. Executable targets still compile normally,
with their ordinary import resolution and entry-point requirements. A failed
interface check cannot replace a previously published build. A published
library package carries its declarations and interface sources and is checked
again when consumed as a project dependency. Existing opaque `.on1xi`
contents must be migrated to the v1 syntax or removed from `On1xfile.i`.

For direct compiler use, pass the interfaces explicitly. For example:

```sh
on1xc --check-interfaces values.on1x --interface values.on1xi
```

For v1, the entry may be a library with no `main`, or a source file that imports the
implementations for multiple `--interface` options. This mode produces no
C/artifact. Ordinary `--check`, `--emit-c` and import lookup do not search for
paired files or read `On1xfile.i`. Unlisted functions remain available under
existing export rules: no signature ascription, sealing, interface-only
imports, ABI fingerprints, persisted interface cache, or separate
compilation is provided.

## Declared module signatures

For declared modules, an explicitly associated `.on1xi` can instead begin
with `abstract module` and the implementation's full hierarchical name:

```on1x
abstract module Acme.Vault;
type Token;
fn make(value: i64) -> Token;
fn read(token: Token) -> i64;
```

Only declaration forms are allowed; a signature cannot import code, contain
bodies, or run a transform. A `type Token;` (also spelled `abstract type Token;`)
requires one original local record or enum named `Token`. `type Box<T>;`
similarly requires a nominal declaration of the same arity. The resulting
abstract identity belongs to the provider, not to the import spelling. Fields,
constructors, and layout are not part of the signature.

Callable declarations require one original local, explicitly typed definition.
A `fn` must match a `fn`; `proc reset(value: i64);` must match a `proc`,
not a `fn` returning `void`. A procedure declaration has no result clause.
Parameter names are not part of the match. The supported types
are numeric scalars, `bool`, `char`, `string`, their tuples, and references to
abstract types listed in the same signature. `void` is permitted only as the
whole result. Generic functions, borrowed parameters, imports and
re-exports do not satisfy these declarations.

Declared-module signature conformance compares source declarations, not
implementation bodies. It never authorizes executable emission. When a
loader uses a callable-only signature as a module boundary, listing a
callable does not increase its visibility, and unlisted members are not part
of that boundary. The current executable HIR cannot carry opaque source
types, so any executable load that reaches an associated signature declaring
an abstract type must reject it instead of exposing its representation.
