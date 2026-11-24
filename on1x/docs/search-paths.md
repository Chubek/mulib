# Library, PPX and MuLIB discovery

This is offline build-time discovery, not runtime loading, a package downloader,
or safe-FFI admission. Existing explicit paths remain supported. The MuLIB
housekeeping package is now integrated at `stdlib/mulib`; its source import
remains `import std.mulib as M;` or `from std import mulib;`.

## Environment and CLI

All new variables and flags accept **colon-separated directory lists**.
Repeat flags to append lists, preserving their order. Empty entries are ignored,
not interpreted as the current directory. Relative entries are relative to the
compiler/build process's working directory. Spaces do not need escaping inside
a quoted argument. The new list syntax uses colons on every host; this increment
is tested on Linux, not Windows drive-letter paths.
The legacy `ON1X_PATH` alias retains its older platform delimiter (semicolon on
Windows, colon on POSIX).

| Environment variable | CLI flag | Used by |
|---|---|---|
| `ON1X_LIB_PATH` | `--lib-path DIR:DIR` | `on1xc`, build helper: ordinary libraries **and manifested stdlibs** |
| `ON1X_STDLIB_PATH` | `--stdlib-path DIR:DIR` | `on1xc`, build helper: standard-library roots |
| `ON1X_PPX_PATH` | `--ppx-path DIR:DIR` | `on1xc`, build helper: PPX, including standard processors |
| `ON1X_STDPPX_PATH` | `--stdppx-path DIR:DIR` | `on1xc`, build helper: preferred standard-processor directories |
| `ON1X_MULIB_PATH` | `--mulib-path DIR:DIR` | Build helper, `on1x-mulib.py`, `on1x-exotract.py`: kernel include directories (or checkout roots containing `kernels/`) |

The build helper is `on1x/tools/on1x-build.py`. It forwards library and PPX
options to the actual compiler; it does not implement another import/manifest
parser. MuLIB inspection/extraction flags precede their subcommand.

```sh
export ON1X_LIB_PATH="/opt/on1x/libraries:$HOME/.local/share/on1x/stdlib"
export ON1X_PPX_PATH="$HOME/.local/lib/on1x/ppx"
export ON1X_MULIB_PATH="/opt/mulib/kernels"

on1xc --use-ppx identity --check app.on1x
python3 on1x/tools/on1x-build.py app.on1x --on1xc build/on1x/on1xc \
  --lib-path local-libs:vendor --ppx-path build/on1x/stdppx \
  --use-ppx identity --mulib-path /opt/mulib/kernels -o build/app
python3 on1x/tools/on1x-mulib.py --mulib-path /opt/mulib/kernels plan bitset
```

Once discovered, source imports use module names, PPXs use processor names, and
`--mulib KERNEL` uses kernel names: no individual source/header/executable path
is required. Existing bridge/intrinsic kernel selection remains automatic.
Arbitrary raw externs still require an explicitly selected kernel and an exact
adapter; discovery does not infer an ABI or synthesize a foreign declaration.

## Order and compatibility

Ordinary imports search the entry source's directory, CLI `-I`/`--module-path`
and `--lib-path` entries (in their combined CLI order), `ON1X_LIB_PATH`, legacy
`ON1X_PATH`, then the resolved standard roots.

`std.*` and the automatic pervasive environment search:

1. Explicit legacy `--stdlib DIRECTORY`, when given (also clears the environment
   `ON1X_STDLIB_PATH` list).
2. CLI `--stdlib-path`, then `ON1X_STDLIB_PATH`.
3. Manifested standard libraries in the general library path list, in order.
4. Legacy `ON1X_STDLIB`, or the relocated executable's `../share/on1x/stdlib`,
   or the configured source-tree standard library.

Each stdlib path can be the manifested directory itself or a container with a
manifested `stdlib/` child. Additional standard roots are fallbacks, not an
implicit wildcard import. `from std import X` and `import std.X` use the same
order. A project's unqualified `X.on1x` cannot shadow `std.X`.
Canonical module identity, relative/quoted imports, namespace-package behavior,
prelude demand loading and import-cycle checks are unchanged.
`--print-stdlib` prints the first resolved standard root without compiling or
executing PPX. `--stdlib-resource RELATIVE_PATH` locates a resource in the same
ordered roots; the build helper uses this instead of duplicating manifest
parsing, including when Basis runtime files come from a fallback stdlib.

PPX discovery searches CLI `--stdppx-path`, `ON1X_STDPPX_PATH`, CLI `--ppx-path`,
`ON1X_PPX_PATH`, standard-library siblings named `stdppx`, then the
executable-relative native processor directory. Dedicated standard lists take
precedence over general lists. First match wins; a malformed matching manifest
fails rather than silently falling through to a later processor.

`--use-ppx NAME` and `--stdppx NAME` explicitly activate a discovered processor.
`--ppx LABEL NAME` also allows a discovered name as its program argument while
retaining the existing two-argument interface. Mixed activation flags retain
their execution order. A source `@ppx label "name";` searches the same directories
if it does not identify an existing source-relative file; source registrations
still require `--allow-source-ppx`. Explicit file paths retain legacy execution
behavior, without requiring discovery metadata. `--use-ppx` never searches CWD
implicitly.

**Discovery alone never runs a PPX.** `--deps` does not run ordinary processors,
and no manifest command is executed. PPX authorization is not sandboxing.
Timeouts, wire/schema validation and post-PPX semantic checks remain in force.
The separate pinned native Category extension registry is **not** redirected by
any of these environment variables or flags.

## Project `On1xfile` version 1

New project manifests use [directive heads and continuation
lines](../stdlib/on1xfile/README.md). The compiler reads only the literal
package identity (`kind`, `name` and optional `directory`) for
search-path discovery. It does not execute Mune, expand a template, select a
build profile, or look inside `On1xfile.m`/`On1xfile.i` to resolve an import.
Project commands, dependency declarations and local directory roles are handled
by the explicit project build driver instead.

The standard-library root contains:

```text
on1xfile 1
kind library
name "std"
directory "."
```

The shipped `stdlib/On1xfile` advertises the whole standard library, including
MuLIB. New standard-search directories require this manifest. Legacy `--stdlib`
and `ON1X_STDLIB` still accept old unmanifested roots.

A library directory can advertise a different source directory:

```text
on1xfile 1
kind library
name "arithmetic"
directory "src"
```

When the manifest is at `SEARCH/arithmetic/On1xfile` or directly at
`SEARCH/On1xfile`, `import arithmetic.number` resolves to `src/number.on1x`
inside that package. `import arithmetic` selects `src/__init__.on1x` or its
namespace directory. Plain `.on1x`/`__init__.on1x` libraries remain compatible.
`directory` defaults to `.`, must name an existing directory inside the package,
and is a literal quoted path without escapes for compiler discovery. New names
start with an ASCII letter or underscore and contain up to 64 ASCII letters,
digits, underscores or hyphens. The old `(On1xfile 1 ...)` package declarations
remain readable for compatibility with existing search paths, not as new build
manifests.

To create a new project, run `on1xc init --bin`, `on1xc init --proj`, or
`on1xc init --lib`. Pass an optional directory instead of using the current
directory. The scaffold refuses to overwrite existing files. From the new
project directory, `on1xc exec build` selects the development profile and
`on1xc exec release` selects the release profile; other command names select a
declared build profile. These commands also work through `tools/on1x-build.py`.
Unlike import discovery, `exec` can evaluate the selected root project's Mune
Lua policy. The driver does not pass the command name or options to a shell.

### Legacy PPX discovery metadata

An already-built native On1x PPX package contains an `On1xfile`:

```lisp
(On1xfile 1 (kind "ppx") (name "my_processors") (protocol "1")
  (processor "identity" "on1x-ppx-identity" "PPX.schema.json"))
```

Each `processor` is `(processor "discovery-name" "executable" ["schema"])`.
A bundle may contain multiple, uniquely named processors. It is searched at the
listed root or `ROOT/NAME/On1xfile`. Executable/schema paths are relative to that
manifest, must exist, and must stay within its directory after symlink resolution.
An optional schema is supplied as `ON1X_PPX_SCHEMA` to that processor; it does not
grant AST extensions or semantic authority. Native source is compiled explicitly,
not auto-built or interpreted during discovery.

CMake generates the native standard PPX bundle manifest in `BUILD/stdppx` and
installs it with its processors/schema under `libexec/on1x`.

### Script PPXs: terse second line

Discovered scripts start with a shebang, then a single metadata comment:

```python
#!/usr/bin/env python3
# on1x: (On1xfile 1 (kind "ppx") (name "identity") (protocol "1"))
```

Also accepted comment prefixes are `// on1x: `, `-- on1x: ` and `; on1x: `.
The metadata must occupy the second line, name the requested processor and
advertise protocol 1; no `processor` entries are allowed in inline metadata.
All 24 shipped Python processors carry this header. Script discovery checks
`NAME`, `NAME.py`, `.sh`, `.lua`, `.pl`, `.rb`, and `.js`, in that order; it never
recursively walks directories or invokes a shell to guess an interpreter.
Explicit-path scripts retain their previous compatibility behavior.

Limits: 256 search directories per family (combined general/standard library
inputs also bounded at 256), 64 KiB per list and file manifest, 4,096 bytes per
search path or script header line, and 128 legacy manifest fields. Existing
S-expression depth/node limits apply to legacy metadata. Missing inputs,
malformed discovery metadata and budget exhaustion reject before
executable/generated-C publication.

## Kernel lookup and evidence

Kernel include lookup searches CLI lists, environment lists, then the selected
checkout's `kernels/`. Metadata/fixtures still come from a real `MANIFEST.yaml`:
explicit `--mulib-root` (`--root` for inspection/extraction), legacy
`ON1X_MULIB_ROOT`, the first search entry's checkout with a manifest, then the
existing source-tree default. Multiple include directories may supply different
selected kernels; lookup does not require spelling individual header paths.

The shared planner records each actual selected `header_path`, hash and include
order. Generated implementation code and Exotract discovery use those selected
headers. Compiler dependency discovery also fingerprints transitive includes.
One generated implementation TU, known companions, source/contract checks and
archive-lock validation remain mandatory. A changed search-path copy of a
source-pinned core header rejects; lookup is not permission to bypass a contract
revision. Unknown kernels without MANIFEST-backed recipes reject rather than
guessing implementation switches or linkage.

Discovery is an **EXECUTABLE Linux subset**, not a safe-import certificate or a
general package system. No new runtime ABI, syntax, native backend, ownership
rule, network fetch, concurrency facility or SDK safety claim is introduced.
See [verification](../VERIFICATION.md) and `tests/test_discovery.py`.
