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
functions/macros and explicitly imported names; selective imports can serve as
small facade modules, as in `stdlib/Int.on1x`.

Absolute named imports search in this order:

1. The entry source file's canonical directory.
2. Repeated `-I DIRECTORY` / `--module-path DIRECTORY`, in command-line order.
3. `ON1X_PATH` directories, in order (colon-separated on POSIX, semicolon on Windows).
4. The standard-library root.

Empty environment path entries are ignored. `std` is anchored directly to the
standard-library root and its sibling `std/` extension directory, so
`import std.Int` cannot accidentally select a project's
`Int.on1x`. `import Int` uses the ordinary search order. Relative named imports
search only the indicated directory. Quoted paths are relative to the importing
file and retain support for explicit absolute paths.

`std.mulib` is implemented in `on1x/std/mulib`; `from std import mulib` uses
the same resolution. A module present in both standard roots is an ambiguity
error. Both roots are installed and relocated together.

An import loads the requested file/initializer and its declared dependencies.
Importing `pkg.child` does not implicitly execute/load ancestor initializers;
use `import pkg` when its re-exports are needed. Directory-only packages provide
submodule discovery, not implicit wildcard exports. Namespace packages use the
first matching search root, rather than merging directories across roots.

Canonical filesystem paths are identities: diamonds, repeated aliases, `..` and
symlinks load one copy. Dependencies are ordered before importers. Cycles report
the importing location and full active chain. There is no persistent process-wide
cache; separate compiler/API invocations see file changes.

## Scope and compilation

Named imports have file-local bindings. Two modules may independently define
`helper` without collisions. The loader assigns collision-free deterministic
internal identifiers and rewrites references with lexical scope awareness,
including parameter patterns, closures, macros, attributes and staged syntax.
Explicit selective re-exports retain the original definition's identity.
Locals and parameters can shadow imported functions and pervasive bindings.
Using `alias.member` after a local shadows `alias` is an error.

Quoted imports preserve the older flat, transitive namespace. Mixing quoted and
named imports of the same canonical file still emits one definition. Conflicting
import bindings are errors. Identical raw extern declarations may be shared;
different source/ABI signatures reject. Raw C symbol spelling and existing
foreign-call checking are preserved.

Modules are compile-time namespaces. Wildcard imports, function-local imports,
runtime module values, dynamic loading, signatures and functors are unsupported.
Overlapping aliases (`a` and `a.b`) should be given distinct `as` names. PPX sees
the flattened v1 AST; names generated by PPX are not source-module exports.
`--types` lists entry/flat-import globals and omits internal module identifiers.

## The pervasive environment

`stdlib/pervasive.on1x` provides `identity`, `ignore`, `fst`, `snd` and `const`.
They are ordinary polymorphic On1x functions backed by
`stdlib/core/Pervasive.on1x`. Automatic definitions are retained only when
referenced, including dependencies of referenced helpers, so unused preludes
do not force inference or enlarge generated programs.

- `--no-prelude` disables the automatic pervasive environment.
- `--prelude FILE` adds an explicitly loaded prelude; repeat for multiple files.
- Explicit imports/declarations take precedence, then later explicit preludes,
  then earlier ones, then the automatic prelude.
- Explicitly imported modules/preludes are fully checked even when unused.

## Standard-library root and installation

`--stdlib DIRECTORY` overrides `ON1X_STDLIB`. Without either, the compiler first
looks beside its executable at `../share/on1x/stdlib`, then uses its configured
source-tree root. A missing automatic prelude is an error, with `--no-prelude`
available for freestanding builds.

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

`on1xc --stdppx pipe --stdppx checks INPUT.on1x` selects the corresponding Python
processors from the `stdppx` directory beside the configured standard library.
It is explicit command-line execution authorization, like `--ppx LABEL PROGRAM`.
The compiler and build helper preserve ordering when these flags are mixed. Source registrations
still require `--allow-source-ppx`, including registrations in imported files.
Native PPXs remain available via `--ppx LABEL EXECUTABLE` and `ON1X_PPX_SCHEMA`.

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
