# On1xfile project builds

`On1xfile` is a UTF-8, line-oriented project manifest. It is **not** an
S-expression or a shell program. It declares project logistics; the
`On1xfile.m` and `On1xfile.i` in each participating directory declare that
directory's file roles. There is no root-wide file-role directive.

```text
# On1xfile
on1xfile 1
kind executable
name "hello"
directory "src"
target "hello" "src/main.on1x"
```

```text
# src/On1xfile.m
on1xfile 1
source "main.on1x"
library "messages.on1x"
```

```text
# src/On1xfile.i
on1xfile 1
interface "messages.on1xi"
```

```on1x
# src/messages.on1xi
on1xi 1;
fn greet(name: string) -> string;
```

The header is mandatory. `#` starts a line or inline comment outside quoted
strings. Quoted fields use JSON string escaping. Project paths are normalized
relative paths with no parent traversal or symlinks in declared inputs.
`directory` defaults to `"."`; discovery walks it for local manifests, pruning
hidden directories, `build`, `target`, and registered dependency stores.
`executable` has exactly one target; `complex` has one or more; all `source`
files must be named targets. Libraries can supply `library` modules to the
canonical loader. A `library` project has no targets and publishes checked
source (not a native archive) under `target/PROFILE/library`. Every declared
`.on1xi` file must match a declared `.on1x` implementation with the same stem
in the same directory. The compiler checks its [v1 function
signatures](../../docs/interfaces.md) against that implementation before
publication, including for dependencies and unused modules. A source package
preserves the checked, fingerprinted interface bytes and declarations.
Interface files are not loaded as program code.

Run from a checkout:

```sh
python3 on1x/tools/on1x-project.py build /path/to/hello \
  --on1xc build/on1x/on1xc --sandbox
```

`--profile NAME` selects a profile, `--release` is shorthand for `release`,
and the default profile is `dev`. `--target NAME` builds only that target;
use `--output-dir DIR` to keep that generation separate from a previous
full build. The default output is `PROJECT/target/PROFILE`. On success the
tool writes its path to stdout. A Linux atomic rename publishes the complete
generation, never an incomplete set after error. A directory not created by
On1xfile is not a valid replacement destination. The `.on1x-build.json`
report records profile settings, selected source and manifest hashes, direct
helper executable identities, and executable artifact hashes.

### Static-analysis scrutiny

A root manifest may set the project-wide fallback policy:

```text
scrutiny standard
```

The value is a lowercase catalog level. The driver forwards it as
`--project-scrutiny`; it does not reinterpret analysis findings. A
`@scrutiny LEVEL;` directive in the file that owns a function overrides this
project value, and `on1x-project.py build --scrutiny LEVEL` overrides both.
The selected value is recorded in `.on1x-build.json`. With the embedded
catalog, the levels are `off`, `standard`, and `strict`; an unknown level is
rejected by the compiler. See [diagnostics and scrutiny](../../docs/diagnostics.md)
for the exact supported Wetstone subset and unavailable-SDK behavior.

`on1x-project.py format PATH [--check]` validates and canonically spaces a
plain root, module, or interface manifest, preserving comment text and order.
It writes to stdout (no overwrite); `--check` fails if bytes would change.
It does not run shell interpolation or Lua. QMR templates are intentionally
not formatted.

### Relative imports and standard-library resources

Manifest paths still allow no `..`. On1x quoted imports may use leading
`../` components (for example, `import "../collections/List.on1x";`) when
the path stays within the importing package's declared `directory` and
resolves to a staged `library` file of that same package. Absolute paths,
interior `..`, `.`, empty components, and symlinked inputs are not allowed.
The compiler still resolves imports and checks the resulting module graph;
the driver does not rewrite source or make new module aliases.

Only a root library named `std` can declare `resource "file"` in an
`On1xfile.m`, and only for the registered Basis and prettyprint C companion
paths and the plain `mulib/On1xfile` discovery manifest. Those files are
hashed and published as data, not compiled by the library build. This does
not let an ordinary dependency supply C sources,
include flags or a link recipe. See [stdlib packaging](../INSTALL.md) for
native resources that still come from the SDK and the downstream commands.

From the project root, `on1xc exec build` and `on1xc exec release` select
the ordinary builds; `on1xc exec NAME` can select a custom profile or a named
source formatter command. For example:

```text
command "listing" format latex "src/main.on1x"
```

`on1xc exec listing > listing.tex` uses the same
[On1x source formatter](../../docs/formatter.md) and `.on1xfmt` as the CLI
without evaluating project policy. Available targets are `plain`, `latex`,
and `troff`. This is separate from formatting the manifest text itself.

## Profiles and manifest preprocessing

The built-in `dev` and `release` correspond to `-O0 -g` and `-O3`.
A root can select its own policy script:

```text
profile "size_debug" "profiles/size_debug.lua"
define entry "main.on1x"
```

```lua
-- profiles/size_debug.lua
return on1xfile.profile { optimize = 1, debug = true }
```

`profiles.lua` defines the profile schema and reusable `on1xfile.profile`
constructor. The policy must return one table with exactly `optimize` (integer
0–3) and `debug` (boolean); raw tool commands and arbitrary flags are not
accepted. A profile is executed only when explicitly selected.

Plain local manifests need no optional helper. A local manifest containing QMR
tags is a trusted template: `on1x-qmr-compile` uses the real LibStone Moonstone
QMR compiler to generate text-only Lua, and `on1x-mune-runner` executes that
text using MuLIB's independent **Mune** kernel, not stock Lua. The adapter
provides `data.profile`, `data.project`, `data.kind` and the root's explicitly
declared `define` string fields. Double braces quote strings as JSON; triple
braces are raw. Statement and comment tags work; includes are unavailable.
The rendered text goes through the same strict role parser as plain manifests.

```text
on1xfile 1
{% if data.profile == "size_debug" then %}
source {{ data.entry }}
{% else %}
source "main.on1x"
{% end %}
```

Both helpers are looked up next to `on1xc` or in its SDK's `libexec/on1x`;
otherwise give `--mune-runner` and `--qmr-compiler`. Outside sandbox mode
`--libstone-root DIR` can compile the actual QMR compiler helper on demand.
Mune runs in a fresh restricted environment with bounded memory, steps,
duration, and output; there is no module loading, filesystem library, `os`,
or random API. These restrictions do not make arbitrary scripts trustworthy.

Shell-derived data requires **both** a root
`define entry shell "printf main.on1x"` directive and operator consent
`--allow-shell`. It executes `/bin/sh` in the real project directory, with a
minimal environment, a timeout, and a bounded result stored as a string. It
can still read or change files and access the network: it is **non-hermetic**
and cannot be combined with `--sandbox`. Dependencies cannot name shell
commands, Mune profiles, PPX, or QMR templates.

## Certified source dependencies and native kernels

```text
dependency "toolkit" local "vendor/toolkit"
dependency "mathlib" git "vendor/mathlib.git" "0123456789abcdef0123456789abcdef01234567"
dependency "parser" git "vendor/parser.git" tag "v2.1.0"
kernel "clamp"
```

Each dependency needs its own plain `On1xfile` of kind `library` and name
matching the declaration, plus local module/interface manifests, e.g.
`import toolkit.util as U;`. `directory "lib"` makes `lib/util.on1x` available
as `toolkit.util`. No build, native binary, script, hook, filter, or project
metadata is run from a dependency. Certification is a checked declaration of
package identity and file roles, **not** a signature, endorsement, or proof
of the behavior of an application that imports the code.

The `git` form refers to a pre-existing, self-contained **local** object store
(bare or ordinary `.git` directory). Its selector can be a full SHA-1/SHA-256
commit ID (the backwards-compatible final quoted field), `commit "ID"`,
`branch "NAME"`, or `tag "NAME"`. Branches and lightweight/annotated tags are
resolved by `on1x-project.py lock` to a commit in `On1xfile.lock`; builds use
that commit and do not follow a moved ref. Annotated tags are peeled to commits.
Ref names use a conservative ASCII subset and cannot begin with an option,
contain traversal/dot-lock syntax, or use Git revision expressions.

The helper projects regular loose objects and pack files into a fresh bare Git
store, checks object integrity, and reads pinned commit blobs. It does not use
checkout data, repository config, hooks, alternates, shallow histories, or
submodules. Projects without Git objects can use `local`; these dependencies
are not immutable, but the actual declared bytes used are recorded. There is
no version solver, transitive manifest resolution, registry, automatic network
fetch during builds, or dependency-specific tooling execution.

`kernel` requests a native MuLIB kernel through the existing single-file C11
builder and the canonical one-TU MuLIB planner. `--mulib-root` selects the
operator's MuLIB SDK. Native requests produce `<target>.mulib.json` with the
canonical link plan. This is not independent KernelBind/FFI admission.

### Locking and explicit remote Git acquisition

```text
dependency "mathlib" git-remote "https://packages.example.org/mathlib.git" "0123456789abcdef0123456789abcdef01234567"
dependency "parser" git-remote "https://packages.example.org/parser.git" branch "stable"
dependency "syntax" git-remote "https://packages.example.org/syntax.git" tag "v2.1.0"
```

Commit pins can be written in the legacy shorthand shown on the first line or
as `commit "ID"`. A branch or tag is mutable manifest intent, so resolve it to
an immutable commit before building:

```sh
python3 on1x/tools/on1x-project.py lock /path/to/project
python3 on1x/tools/on1x-project.py fetch mathlib /path/to/project
python3 on1x/tools/on1x-project.py build /path/to/project --frozen
```

`lock [PROJECT]` resolves and validates all direct Git dependencies, fills the
remote object cache, and atomically writes canonical JSON to `On1xfile.lock`.
`fetch NAME [PROJECT]` does the same for one named `git-remote` dependency and
merges its entry into the lockfile. `PROJECT` defaults to the current directory.
Both commands accept `--ca-file PEM`. They run on the host, not in the build
sandbox; review each endpoint before authorizing network access.

The lockfile has schema `on1x.lock/v1`. Each entry records the dependency kind,
exact source, manifest selector, and resolved full commit ID. It is intended to
be committed. For example:

```json
{
  "dependencies": {
    "parser": {
      "commit": "0123456789abcdef0123456789abcdef01234567",
      "kind": "git-remote",
      "selector": {"kind": "branch", "value": "stable"},
      "source": "https://packages.example.org/parser.git"
    }
  },
  "schema": "on1x.lock/v1"
}
```

Regular and sandbox builds never contact a remote. A selector declaration
requires a matching lock entry; a legacy commit pin can still build directly
for compatibility. `build --locked` requires complete matching lock entries
even for commit pins. `--offline` makes the normal network prohibition
explicit. `--frozen` means `--offline --locked`. The same flags on `lock` and
`fetch` reuse locked cache data (`--offline`), prohibit lockfile changes
(`--locked`), or do both (`--frozen`). An ordinary `lock` or selector `fetch`
refreshes branch/tag intent; a frozen operation never does.

Only **smart HTTPS** with TLS certificate and hostname verification is
admitted: use a DNS hostname or IPv4 address, optional port, and a repository
path with simple ASCII URI segments. No userinfo, escaped or dot segments,
query, fragment, redirects, proxies, SSH, `file`, plain HTTP, arbitrary
remote helpers, or dumb HTTP object crawling. The operator can explicitly
provide a trusted PEM CA bundle (at most 1 MiB); otherwise Git uses the
system CA defaults. Git sees a clean environment and a private home. Local,
user and system Git configuration, credential helpers, askpass, ambient
proxy/TLS variables and `.netrc` credentials aren't inherited. Authenticated
servers, shallow or partial history, and servers that refuse the requested
full commit/ref are not supported. SSH, `git://`, arbitrary remote helpers and
absolute `file://` URLs remain unsupported; use the normalized project-relative
`git` form for a local bare or ordinary repository. Abbreviated object IDs and
implicit fallback to a different reference are never accepted. Network
diagnostics escape terminal controls and redact the admitted endpoint URL.

On Linux, the command atomically publishes only the checked object files
under `PROJECT/.on1x-deps/NAME/COMMIT` and prints that path. Treat `.on1x-deps`
as a cache, not project source. `(NAME, COMMIT)` is the deterministic cache key.
A per-project advisory lock serializes writers; no-clobber rename publishes a
whole generation, and a concurrent winner is revalidated. Lockfiles are also
replaced atomically. Corrupt or conflicting generations fail closed rather
than being overwritten.

The commands never check out the repository or run dependency scripts, hooks,
filters, or repository-configured commands. They pass arguments directly to
Git without a shell and use a clean environment/private home. Gitlinks may be
present as inert tree entries, but submodules are never initialized, fetched,
or admitted as declared regular source files. Acquisition
checks the exact commit with the same offline integrity, library-name and
file-role checks used during builds. It does not compile the library or
prove its behavior: builds still enforce the module, source and compiler
checks. A cached generation is revalidated offline; corrupt or conflicting
stores are not overwritten. To reacquire one, remove that cached generation
and run `fetch` again.

For `local`, current declared bytes determine the build and are fingerprinted.
For `git`, only committed blobs at the selected/locked commit are visible:
dirty or untracked checkout files and the checked-out branch are deliberately
ignored. A missing local ref, garbage-collected locked commit, malformed store,
stale lock source/selector, unavailable network, TLS/authentication failure and
missing cache have distinct diagnostics.

The network Git subprocess has a 30-second wall limit, a 64 MiB pack file
limit, and a 512 MiB per-process address-space limit. Offline verification
also bounds the projected object store to 128 MiB and 32,768 object files,
each Git subprocess to 20 seconds (30 seconds total for acquisition
validation), and the declared input snapshot to 4,096 files/64 MiB. The
history reachable from the pinned commit must fit these limits; use a
pre-materialized local store when remote acquisition is unavailable.

## What `--sandbox` does and does not guarantee

Default project builds stage only declared inputs (plus access to
operator-selected SDK libraries), discard ambient environment/search paths,
verify the module graph, and do not perform network fetching or implicit shell
interpolation. The default does **not** OS-isolate the compiler or host tools.
Ordinary source PPX and executable source extensions are rejected; this is a
restricted build profile rather than a replacement for `on1x-build.py INPUT`.

`--sandbox` fails closed if Linux bubblewrap cannot create the requested
namespaces. It runs the compiler, native builder, and selected policy helpers
with a fixed working layout, a clean environment, read-only staged inputs,
mounts for the SDK and `/usr` toolchain, isolated network namespace, and
private `/tmp`, `/proc`, and `/dev`. The host project and home directory are
not mounted as build inputs. The host driver still performs trusted filesystem
acquisition, offline Git object validation and output publication outside the
process sandbox. The pinned On1x SDK/compiler binaries and host `/usr`
toolchain are trusted and not automatically distributed or version-locked.
This is process isolation, not a certified reproducible toolchain: artifact
equality across environments or changes to the SDK/native toolchain is not
guaranteed. An unoptimized debug build may include ephemeral generated-C
paths unless the builder is configured to remap them. Script correctness,
races with untrusted local filesystem writers, and hostile project execution
after linking are outside the guarantee.
