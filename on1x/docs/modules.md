# Modules, pervasive bindings and library loading

The [nominal-data profile](nominal-adts.md) adds a separate type namespace to
these existing import/re-export rules. Type annotations and explicit constructors
resolve to the declaring module, including through aliases; equally shaped
declarations in different modules are not interchangeable. Unknown/unimported
types reject. This is still whole-unit loading, not separate compilation or a
persisted module interface ABI.

The opt-in [Category declaration gateway](category.md) expands native, local
scalar declarations before export binding. Generated selectors are ordinary
checked exports; protocol/witness metadata is not yet an imported interface.
Source feature activation is module-local, while CLI activation applies to the
compilation unit. This does not add module signatures, functors or sealing.

On1x resolves imports at compile time, before hygienic macros, PPX, staging,
inference and ordinary checking. Generated programs contain C11 and the selected
runtime adapters. Module objects do not exist at runtime.

## Source syntax

```on1x
import std.Int as Int;
from std import String, List;
from std.Functional import compose as chain;

fn twice(x:i64)->i64 { return x*2; }
fn increment(x:i64)->i64 { return x+1; }
fn main() {
  println(identity(chain(increment,twice)(20)));
  println(Int.div(-7,3));
  assert(String.contains("On1x modules","modules"));
}
```

Supported forms (statements end in `;`):

| Form | Binding |
|---|---|
| `import alpha;` | `alpha.function(...)` / `alpha.macro!(...)` |
| `import pkg.alpha;` | `pkg.alpha.function(...)` |
| `import alpha as a, beta as b;` | `a.function(...)`, `b.function(...)` |
| `from alpha import function, other as local;` | Direct names in this file |
| `from alpha import (function, other,);` | Parenthesized multi-line list |
| `from pkg import child;` | An explicit export or the `child` submodule |
| `from .child import function;` | Relative to this file's package directory |
| `from .child import (operator("<+>"));` | Select an exported intrinsic operator by its first lexeme |
| `from ..child import function;` | One directory above it |
| `from . import child;` | Sibling submodule, including inside `__init__.on1x` |
| `import "relative/file.on1x";` | Compatibility flat import |

Qualified expression names have adjacent dots (`a.b`, rather than `a . b`).
This preserves spaced dotted-tail syntax in structural macros. `from` and `as`
are contextual words; existing local identifiers using them remain valid.

## Resolution and identity

A named module maps to `name.on1x`, `name/__init__.on1x`, or a directory-only
namespace package. Having both the file and initializer at one search location
is an ambiguity error. Empty initializers are valid. A file exports its own
functions, macros, module-scope operators, and explicitly imported names.
Selective imports can serve as small facade modules, as in `stdlib/Int.on1x`.

Absolute named imports search in this order:

1. The entry source file's canonical directory.
2. Repeated `-I DIRECTORY` / `--module-path DIRECTORY` and colon-separated
   `--lib-path` lists, in command-line order.
3. `ON1X_LIB_PATH`, then legacy `ON1X_PATH` directories.
4. The resolved standard-library roots.

Empty environment path entries are ignored. `std` is anchored to the
resolved standard-library roots (including manifested stdlibs discovered through
`ON1X_LIB_PATH`), so
`import std.Int` cannot accidentally select a project's
`Int.on1x`. `import Int` uses the ordinary search order. Relative named imports
search only the indicated directory. Quoted paths are relative to the importing
file and retain support for explicit absolute paths.

`std.mulib` is implemented in `on1x/stdlib/mulib`; `from std import mulib` uses
the same resolution. Legacy sibling `std/` trees remain readable for headerless
modules. They cannot provide or override a declared standard module.

A declared `std.X.Y` uses `X/Y.on1xm` or `X/Y/__init__.on1xm` inside a selected
standard root and requires a same-stem `.on1xi` signature. Only `--stdlib`,
`--stdlib-path`, their standard-library environment counterparts, and the
configured or installed default can supply declared standard modules. General
library paths still support old, headerless standard-library manifests.
Project membership cannot claim `std` or `std.*`.

`Y.fragments/` (or `__init__.fragments/` for an initializer) can provide
additional `.on1xm` files with the same declaration. The primary file comes
first, then the fragments in path order. Neighboring `.on1xi` files in the
fragment directory contribute to the same signature. The signature is verified
against the joined implementation on ordinary builds.

When a declared standard module replaces a headerless body, signed members
remain available through its old dotted, relative and same-stem quoted import
paths in the selected root. Quoted imports open only accessible signature
exports, not the original implementation namespace, and all aliases use the
same definitions. Headerless facades can still open and re-export those names;
a duplicate legacy body in the same root is an ambiguity, not a facade.

An import loads the requested file/initializer and its declared dependencies.
Importing `pkg.child` does not implicitly execute/load ancestor initializers;
use `import pkg` when its re-exports are needed. Directory-only packages provide
submodule discovery, not implicit wildcard exports. Namespace packages use the
first matching search root, rather than merging directories across roots.

Canonical filesystem paths are identities: diamonds, repeated aliases, `..` and
symlinks load one copy. Dependencies are ordered before importers. Cycles report
the importing location and full active chain. There is no persistent process-wide
cache; separate compiler/API invocations see file changes.

## Unused bindings

The compiler reports unused namespace and selective import bindings as
`unused-import` in diagnostic group `unused`. It identifies what was imported
(for example, a function, procedure, module, type or resource). A **written**
alias starting with `_` opts out: `import Data as _data` or
`from Data import fetch as _fetch`. An unaliased `_data` import is not opted
out. `as _` is an error (`invalid-import-alias`, group `imports`), including
for selector aliases; hiding the diagnostic cannot make the source valid.

Source variables, including parameters and pattern binders, use
`unused-variable` in the same group. Use `_` for an ignored binding or prefix
a binder with `_` to opt out. Only a visible **outer** variable is suppressed
when shadowed; a new unused inner variable may itself be reported. A
nonrecursive `let` or `var` introduces its binding after its initializer. Block,
lambda, match arm and loop scopes do not extend past their lexical boundaries.
Plain assignment does not read the destination. A label or field name is not
a lexical value reference. For `unused-type`, both private records and private
enums are checked, including generic declarations. Their own representations
do not count as a use of themselves; references from other declarations do.
Public, shared and unannotated declarations are not reported.

These are source-level observations rather than reachability or post-expansion
lint. Macro definition-site references and references explicitly passed in
macro arguments count; copied macro binders are not reported. The report does
not interpret whether a macro will expand or keep its argument. It does not run
PPX for discovery; source graphs using PPX, attributes or declaration
extensions are excluded. Internal compiler bindings, standard-library files,
preludes and their prelude-only dependencies, and the source of an imported
program whose entry point has been removed are not analyzed for findings.
Legacy quoted includes open a flat scope and do not introduce one reportable
namespace binding; an
indistinguishable legacy re-export or repeated import of the same binding is
conservatively counted as used if a reference resolves to that binding. The
ordinary loader and checker still enforce admission and type semantics.

## Scope and compilation

Named imports have file-local bindings. Two modules may independently define
`helper` without collisions. The loader assigns collision-free deterministic
internal identifiers and rewrites references with lexical scope awareness,
including parameter patterns, closures, macros, attributes and staged syntax.
Explicit selective re-exports retain the original definition's identity.
The [operator namespace](operators.md) also uses explicit selective imports
and re-exports; operator notation and references are never module-qualified.
Locals and parameters can shadow imported functions and pervasive bindings.
Using `alias.member` after a local shadows `alias` is an error.

Quoted imports preserve the older flat, transitive namespace. Mixing quoted and
named imports of the same canonical file still emits one definition. Conflicting
import bindings are errors. Identical raw extern declarations may be shared;
different source/ABI signatures reject. Raw C symbol spelling and existing
foreign-call checking are preserved.

Modules are compile-time namespaces. Wildcard imports, function-local imports,
runtime module values, dynamic loading and runtime functors are unsupported.
[Project interfaces](interfaces.md) include legacy checks and static signature
ascription. Checked C11 can carry eligible [opaque module
values](opaque-module-abi.md) without letting callers use the representation.
Overlapping aliases (`a` and `a.b`) should be given distinct `as` names. PPX sees
the flattened v1 AST; names generated by PPX are not source-module exports.
`--types` lists entry/flat-import globals and omits internal module identifiers.

## The pervasive environment

`stdlib/pervasive.on1x` provides ordinary polymorphic declarations backed by
`stdlib/core/Pervasive.on1x`:

| Group | Unqualified names |
|---|---|
| Basic functions | `identity`, `ignore`, `fst`, `snd`, `const`, `apply`, `compose`, `pipe`, `flip` |
| Nominal types and constructors | `Option<T>`, `Result<T,E>`, `option_some`, `option_none`, `result_ok`, `result_error` |
| Option queries/transforms | `option_is_some`, `option_is_none`, `option_map`, `option_and_then`, `option_filter` |
| Option fallback/conversion | `option_unwrap_or`, `option_unwrap_or_else`, `option_ok_or`, `option_ok_or_else`, `option_iter` |
| Result queries/transforms | `result_is_ok`, `result_is_error`, `result_map`, `result_map_error`, `result_and_then`, `result_or_else` |
| Result fallback/conversion | `result_unwrap_or`, `result_unwrap_or_else`, `result_ok_option`, `result_error_option`, `result_iter`, `result_iter_error` |
| Operator bindings | `\|>`, `<\|`, `>>.`, `<<.`, `<$>`, `<&>`, `>>=`, `=<<`, `>=>`, `<=<` |

The types are `enum Option<T>{None,Some(T)}` and
`enum Result<T,E>{Ok(T),Err(E)}`. Constructor helpers infer payload arguments;
payload-free `option_none()`, the unused error argument of `result_ok(...)`, and
the unused success argument of `result_error(...)` need a contextual type
annotation or return type. `_or` arguments are evaluated normally before the
call, while `_or_else` callbacks are invoked only for the fallback branch.
Maps, chains, filters and iteration callbacks likewise run only for their
selected variant. Postfix `?` applies to `Result`, not `Option`.

These nominal values retain the bounded data profile. A concrete pervasive
`Result<T,E>` may use the separate finite `own<Named>` profile in `T` or `E`;
direct construction, consuming match and postfix `?` then use checked moves and
lexical cleanup. The generic helper functions above remain copy-safe because
On1x has no ownership trait bound, so they reject rather than copy a move-only
specialization. Managed strings, closures, buffers, S-expressions and general
foreign resources remain restricted. This adds no runtime ABI. The explicit
`std.Option` tagged-tuple, `std.extensions.Result` scalar-pair and
`std.base.{option,result}` S-expression APIs remain separate compatibility
representations.

Automatic definitions are retained only when referenced, including dependencies
of referenced helpers, so an unused prelude does not force inference or enlarge
generated programs.

- `--no-prelude` disables the automatic pervasive environment, including
  [pervasive operators](operators.md#standard-library-operators).
- `--prelude FILE` adds an explicitly loaded prelude; repeat for multiple files.
- Explicit imports/declarations take precedence, then later explicit preludes,
  then earlier ones, then the automatic prelude.
- Explicitly imported modules/preludes are fully checked even when unused.
- A local or explicitly imported `Option`, `Result` or helper can therefore
  shadow the automatic declaration. Pervasive helpers always construct the
  pervasive nominal types; shadow all related names when defining a replacement
  facade.

## Standard-library root and installation

New `--stdlib-path`/`ON1X_STDLIB_PATH` and manifested general-library paths
precede the default library. `--stdlib DIRECTORY` retains explicit-root priority
and overrides `ON1X_STDLIB`. Without overrides, the compiler first
looks beside its executable at `../share/on1x/stdlib`, then uses its configured
source-tree root. A missing automatic prelude is an error, with `--no-prelude`
available for freestanding builds.

The shipped root has an `On1xfile` manifest; new standard-library search
directories require one. Plain modules and legacy explicit roots remain
compatible. See [search paths and manifests](search-paths.md) for exact order,
directory-list syntax, package source roots and bounded validation.

```sh
cmake -S on1x -B build/on1x
cmake --build build/on1x --parallel 4
cmake --install build/on1x --prefix /your/prefix
```

Installation includes `bin/on1xc`, library/runtime/schema resources under
`share/on1x`, the build helper under `share/on1x/tools`, and enabled native PPXs
under `libexec/on1x`. The prefix may be relocated as a unit.

```sh
python3 on1x/tools/on1x-build.py on1x/examples/modules.on1x \
  --on1xc build/on1x/on1xc -o build/modules
```

The helper forwards loader flags and links the supplied Basis C11 adapters and
timing adapter once when their emitted extern declarations are present. Array
handles still require explicit destruction; these adapters use the existing raw
FFI contract. The Basis inventory continues to distinguish partial adapters from
unimplemented SML signatures/functors.

## PPX lookup

`on1xc --stdppx pipe --stdppx checks INPUT.on1x` selects processors by name.
`--use-ppx NAME` is an equivalent activation flag. Dedicated
`--stdppx-path`/`ON1X_STDPPX_PATH` and general `--ppx-path`/`ON1X_PPX_PATH`
precede standard-library sibling and executable-relative defaults.
It is explicit command-line execution authorization, like `--ppx LABEL PROGRAM`.
The compiler and build helper preserve ordering when these flags are mixed. Source registrations
still require `--allow-source-ppx`, including registrations in imported files.
Discovered native PPXs use `On1xfile` bundle metadata; discovered scripts use
terse second-line manifests after their shebang. Native PPXs also remain
available via explicit `--ppx LABEL EXECUTABLE` and `ON1X_PPX_SCHEMA`.

## API, diagnostics and limits

`include/on1x/loader.hpp` exposes `LoaderOptions`, `LoadedUnit`, `load_unit()` and
`default_loader_options()`. Embedders can supply roots and smaller budgets without
reading environment variables. A failed call publishes no partial unit.

`on1xc --deps INPUT.on1x` prints canonical source dependencies, one path per line,
without running PPX or type checking. Automatic prelude files are included even
when their definitions are unused. Existing output files are written only after
successful loading/compilation.

Default limits: 1,024 modules (including namespace packages), 64 active load
levels, 16 MiB aggregate source, 2 MiB per file, 1,024 import declarations per
file, 16,384 dependency edges, 262,144 import bindings and 16,384 loaded functions.
Existing parser, PPX, macro, inference and runtime budgets apply subsequently.

Tests: `on1x.loader_api`, `on1x.modules`, `on1x.libraries`, `on1x.stdppx`,
`on1x.stdppx_protocol`, and `on1x.install`, alongside the language regressions.
