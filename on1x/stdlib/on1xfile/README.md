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

The `on1xfile 1` header is mandatory. A known, unquoted directive at the
start of a physical line (after optional indentation) starts a new statement.
Other fields continue the previous statement. Quoted directive names are
fields, not heads. Blank lines and comments do not end a statement. `#`
starts a line or inline comment outside quoted strings; JSON string escapes
work as before, and a quoted field cannot cross a physical line.

For example, the following two directives select a local Git repository
with a full commit pin and the `mir-c11` backend:

```text
dependency "mathlib" git
  "vendor/mathlib.git"
  "0123456789abcdef0123456789abcdef01234567"
backend mir-c11
```

A trailing `\` outside a quoted string or comment explicitly continues the
statement onto the next line containing fields, even if its first word looks
like a head. Only spaces, tabs, or the CR in CRLF may follow the backslash;
`\ # comment` is an error, and a backslash inside a comment does nothing.
The marker must follow a field on the same line and cannot dangle at EOF,
even with blank or comment-only lines after it. For example, write
`kind library` on one line or:

```text
kind \
  library
```

Without the marker, `library` at the start of a line is a known file-role
head; it is not a field of `kind`. Heads are recognized even where they are
not allowed, such as root-only directives in a dependency. The parser does
not guess a second grouping or prompt. If a statement fails, diagnostics
show `path:line` or an inclusive `path:start-end` from its head through its
last field line (or the physical line with the lexical error). Put an
intended directive first on its own line; quote a string field, or move a
directive-looking bare field up or use an explicit continuation. Build,
fetch, and lock fail without waiting for input.

The same rules apply to `On1xfile.m`, `On1xfile.i`, plain `init` templates,
and the text produced by an authorized QMR template. `format` preserves
wrapping, comments, and explicit continuation markers, normalizes line
endings and spacing (two spaces before continuation fields), and removes
trailing blank lines.

Project paths are normalized relative paths with no parent traversal or
symlinks in declared inputs.
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
  --on1xc build/on1x/on1xc --cc cc --sandbox
```

`--profile NAME` selects a profile, `--release` is shorthand for `release`,
and the default profile is `dev`. `--target NAME` builds only that target;
use `--output-dir DIR` to keep that generation separate from a previous
full build. The default output is `PROJECT/target/PROFILE`. On success the
tool writes its path to stdout. A Linux atomic rename publishes the complete
generation, never an incomplete set after error. A directory not created by
On1xfile is not a valid replacement destination. The `.on1x-build.json`
report records profile settings, selected source and manifest hashes, direct
helper executable identities, and executable artifact hashes. For a source
library, `outputs` maps every published package file to its SHA-256 digest,
with paths relative to the output directory (for example,
`library/lib/value.on1xi`). This includes any registered resources and the
normalized manifests as published, not the original manifest bytes recorded
in `inputs`. The report does not hash itself and is limited to 16 MiB.

### Backend selection

Choose one backend for the project, optionally overridden for individual
built-in or custom Lua build profiles:

```text
backend c11
backend "debug" mir-vm
backend "release" mir-archon
```

`on1xc exec build --profile debug` uses the scoped `mir-vm` entry.
`on1xc exec release --backend mir-c11` overrides the scope for that build.
Precedence is **CLI > selected profile > project-wide > c11**; `--via-mir`
is an alias for `--backend mir-c11`. A project never falls back to another
backend. Check a selected toolchain and see its artifact kind, tools and flags
before building with `on1x-project.py plan PROJECT --backend NAME`.
`on1x-project.py backends --on1xc /path/to/on1xc` probes all four backends
with a fixed program and prints availability or a failure reason. It uses
the host compatibility driver for the C11 probes, does not read the current
project's manifest, and does not execute the compiled programs.

| Backend | Project output | Requirements |
|---|---|---|
| `c11` | Host executable from direct HIR→C11 | C compiler and reference runtime |
| `mir-c11` | Host executable from verified MIR→C11 | MIR-admitted source and C toolchain |
| `mir-archon` | Linux AMD64 SysV ELF executable | Archon generator tool, matching target manifest, assembler/linker, scalar ABI and runtime archives |
| `mir-vm` | `NAME.o1vm` bytecode, bundled `.on1x-vm-runner`, executable `NAME` launcher | VM-enabled compiler and matching `on1x-vm` beside it; no host C compiler |

Hosted `c11` and `mir-c11` projects select Cephyr, SandAS, and Yellowstone ld
by default. See [native project tools](../../docs/native-project-toolchain.md)
for root directives, per-tool arguments, current limits, and the explicit host
compatibility path.

The three MIR backends use checked MIR and reject constructs outside their
admitted profiles. Source-library packaging uses the checked source path.
`mir-archon` and `mir-vm` do not link project `kernel` declarations. The VM
runner verifies the emitted tape before publication. Copy the **entire VM
output directory**, including the hidden runner, to relocate it on a compatible
host. The generated launcher uses `/bin/sh`; you can also run
`./.on1x-vm-runner ./NAME.o1vm` directly. The output report hashes every VM
artifact. Project source locations in bytecode use `/__on1x__/project`,
`/__on1x__/deps`, `/__on1x__/stdlib` and `/__on1x__/std`, preserving
line/column without embedding temporary staging paths. VM builds record an
empty `cflags` array because no C build occurs.

For single-file builds, `on1x-build.py --backend mir-vm INPUT -o app.o1vm`
publishes bytecode (run it with `on1x-vm app.o1vm`); `--backend mir-archon`
publishes the ELF executable, or assembly with `--emit-only`. These paths
also use the selected compiler's adjacent tools. Native plans probe the real
target/ABI and fingerprint the assembler, linker, target manifest and archives.

### ArchVirt launch policy

An executable or complex project's root `On1xfile` can choose an ArchVirt
launcher with exactly one of:

```text
vm opt-in
```

```text
vm always
```

`build` compiles and publishes a launcher for each selected target. It does
not start the guest. A launcher built with `vm opt-in` requires an explicit
`./target/dev/NAME --sandbox`; without `--sandbox` it refuses to run.
A `vm always` launcher attempts to start its embedded guest on direct
invocation. Neither policy produces or falls back to a host implementation of
the guest program. This mechanism is distinct from the `mir-vm` bytecode
backend above.

```sh
on1x-project.py lock .
on1x-project.py plan . --frozen
on1xc exec build --frozen
on1xc exec run --frozen --target hello
```

`on1xc aux run PROJECT` (also `on1xc exec run` from the root) is the explicit
build-and-launch command for either policy. It requires one target; use
`--target NAME` for a multi-target project. It compiles a guest ELF in a
private build directory, then validates, caches and runs that exact image
through ArchVirt. It returns the guest status without publishing or replacing
a build generation. `run` with no root VM policy refuses to start anything.
The initial guest profile does not support argument passing: arguments after
`--` are rejected, not dropped or reinterpreted as launcher options.

`run` reports the VM ID on stderr as `ArchVirt cache key: KEY`.
Use `on1xc aux run PROJECT --prepare --frozen` to register the same image
and report its ID without starting the VM. Replay the exact cached guest
or clear that cache explicitly:

```sh
on1x-project.py vm-rerun PROJECT KEY --frozen
on1x-project.py vm-cache-clear PROJECT --frozen
# From the project root:
on1xc exec vm-rerun KEY --frozen
on1xc exec vm-cache-clear --frozen
```

Replay and clear validate the root manifest and applicable lock, but do not
compile, load dependency manifests, or read current sources. A missing or
invalid key fails without rebuilding. These commands use the compiler's
per-user cache, not a project-owned cache: they honor the caller's absolute
`XDG_CACHE_HOME` or `HOME`, and `vm-cache-clear` clears that user's ArchVirt
replay cache, not just keys printed by this project. Neither `build` nor
invoking a published linked launcher populates the replay cache. A linked
launcher embeds its guest and does not depend on cached files. `run`,
`vm-rerun`, and `vm-cache-clear` are reserved command/profile names.

Dependencies, library roots, discovered library package metadata, and
directory-local `On1xfile.m`/`On1xfile.i` files do not set VM policy.
Nested `On1xfile` files within the declared source tree are not evaluated as
projects, and a literal `vm` directive in one is rejected.
A VM project cannot declare a hosted backend or native kernels, or override
the backend with `--backend`/`--via-mir`. The launcher build uses the
compiler's ArchVirt toolchain, not the host project `--cc` or C profile
flags. The driver admits only processor optimization selections for this
path and rejects `--optimize-ir`. Unsupported guests or unavailable VM
tools are build or launch errors; `plan` reports policy and validates
inputs but does not build or probe a launcher.

The driver's `--sandbox` still means fail-closed Linux bubblewrap isolation
for the **build**. After the namespace check succeeds, a trusted host-side
query to the selected compiler enumerates regular ArchVirt toolchain files
without project source or policy input. The driver adds those files as
read-only mounts, not a build tree or a KVM device. It is not
the same as the opt-in launcher's `--sandbox` switch. The explicit `run`
command invokes ArchVirt outside the build namespace only after the staged
guest has compiled. Replay and clear do not create a build namespace. A
failure at any stage does not trigger a host fallback.

`lock` records `vm` in `on1x.lock/v3`, even for projects without Git
dependencies. `build`, `plan`, `run`, `vm-rerun`, and `vm-cache-clear`
with `--locked` or `--frozen` require the lock to match; even ordinary
invocations reject a stale lock when one is present. Re-lock after adding,
changing, or removing `vm`. `--frozen` retains the existing offline dependency
behavior and does not itself launch a target or authorize any host backend.

### Compiler optimizations

Use `on1xc --list-optimizations` for the active compiler's generated rule
names, lowering options, and handwritten MIR passes. Declare policy in the
root file; dependencies cannot set compiler policy. For example:

```text
backend mir-c11
optimization-profile default
optimization "integer_add_zero_right" on
optimization "dev" "all" off
optimization "dev" "integer_add_zero_right" on
optimization "release" "local-slots" off
```

Only the MIR backends accept named lowering or MIR passes.
With a Novostone-enabled compiler, `optimization "novostone-hir" on`
selects the [checked-HIR pass](../../hir/README.md) on any project backend.
`optimization "firestone-hir" on` works the same way for the
[Firestone pass](../../hir/README.md#firestone).

**Project `--profile` selects the build profile** (output directory, C flags,
and optional Lua settings). **`--opt-profile default|debug|release` selects
the compiler policy**, independently of C flags. The compiler's `default`
keeps generated rewrites and decision SSA enabled, with MIR passes opt-in
(`--optimize-ir` on the MIR path); `debug` turns all optional optimizations
off; `release` enables the standard optimizations for applicable stages.
`novostone-hir` and `firestone-hir` stay off unless explicitly selected. Original typing,
ownership, semantic and MIR verification still run.

Without a manifest or CLI compiler profile, built-in `release` and `debug`
build profiles use the matching compiler preset. `dev`, `default` and
custom Lua build profiles use `default`. The manifest `optimization-profile`
sets the base for **all** build profiles, overridden only by CLI `--opt-profile`.
`dev`, `debug` and `default` use `-O0 -g`; `release` uses `-O3`.
Existing explicitly declared custom profiles named `debug` or `default`
retain their own C flags.

The two-field `optimization "NAME" on|off` applies project-wide.
`optimization "BUILD_PROFILE" "NAME" on|off` applies only to the named
built-in or declared custom build profile. Apply project-wide selections
first, that build profile's selections second, and CLI `--enable-opt NAME`
or `--disable-opt NAME` last. Within a layer, `"all"` sets the baseline and
named selections override it, independent of order. A higher layer selecting
`all` resets lower-layer selections. Duplicate manifest declarations for a
selector in one scope and contradictory flags in one CLI request are errors.

```sh
on1xc exec build --profile debug --enable-opt integer_add_zero_right
on1xc exec release --via-mir --disable-opt decision-ssa
python3 on1x/tools/on1x-project.py plan . --opt-profile release --disable-opt local-slots --via-mir
python3 on1x/tools/on1x-project.py lock . --on1xc /path/to/on1xc
```

`plan` validates declared inputs and prints JSON without emitting targets.
The plan and `.on1x-build.json` include the declared scopes, resolved compiler
flags, available optimizations and enabled state, active backend stages, and
compiler identity. Enabled entries for inactive stages do not run. A project
with optimizer/backend directives writes an `on1x.lock/v2` file recording
the manifest policy; without directives, dependency locks keep the original
v1 format. Changes to locked declarations require re-locking. CLI overrides
are recorded in the build/plan and do not rewrite the dependency lock.
`format --on1xc PATH`, `lock`, and `build` validate names against the linked
compiler; formatting without a compiler validates the declarative syntax.

The same `--opt-profile`, `--enable-opt` and `--disable-opt` switches work in
`on1x-build.py` and the Archon `on1x-native-build.py` helper. For single-file
compilation `--profile` aliases `--opt-profile`. KernelBind discovery and
reviewed/kernel C11 profiles reject explicit optimization policy.

### Static-analysis scrutiny

A root manifest may set the project-wide fallback policy:

```text
scrutiny standard
scrutiny-work 8192
```

The value is a lowercase catalog level. The driver forwards it as
`--project-scrutiny`; it does not reinterpret analysis findings. A
`@scrutiny LEVEL;` directive in the file that owns a function overrides this
project value, and `on1x-project.py build --scrutiny LEVEL` overrides both.
The selected value is recorded in `.on1x-build.json`. With the embedded
catalog, the levels are `off`, `standard`, and `strict`; an unknown level is
rejected by the compiler. See [diagnostics and scrutiny](../../docs/diagnostics.md)
for the exact supported Wetstone subset and unavailable-SDK behavior.

`scrutiny-work` sets the compiler's optional analysis budget per invocation
(an unquoted decimal integer from `1` to `10000000`). `on1xc exec build
--scrutiny-work N` overrides it for that build. The effective explicit budget
is recorded as `scrutiny_work` in `plan` and `.on1x-build.json`. If neither
setting is supplied, the compiler uses its default. The budget does not limit
mandatory type, ownership, or borrow checking. Dependency manifests cannot set it.

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
profile-arg "size_debug" level integer 1
profile-arg "size_debug" symbols boolean true
profile-arg "size_debug" flavor string "ffi"
define entry "main.on1x"
```

```lua
-- profiles/size_debug.lua
local context = on1xfile.context()
local level = on1xfile.argument("level")
local symbols = on1xfile.argument("symbols")
if context.backend == "c11" and on1xfile.argument("flavor") == "ffi" then
    level = math.min(level, 1)
end
return on1xfile.profile { optimize = level, debug = symbols }
```

`profiles.lua` defines the profile schema and reusable `on1xfile.profile`
constructor. The policy must return one table with exactly `optimize` (integer
0–3) and `debug` (boolean); raw tool commands and arbitrary flags are not
accepted. A profile is executed only when explicitly selected.

`profile-arg "profile" key TYPE value` declares a root-only argument for a
named Lua profile in the same manifest. It may appear before or after the
`profile` declaration. Keys use `[A-Za-z_][A-Za-z0-9_]*` (at most 64 characters),
must be unique within that profile, and are limited to 64 declarations per
manifest. The types are `string` (quoted), `integer` (unquoted signed 64-bit
decimal, no leading zeros or negative zero), and `boolean` (unquoted `true` or
`false`).
`on1xfile.argument("key")` returns the typed value or `nil` for a missing key;
it does not coerce strings or replace `false` with a default. There are no
command-line or environment overrides. These arguments are separate from
`data` and are not exposed to QMR.

`on1xfile.context()` returns a fresh snapshot of read-only project metadata:

| Field | Value |
| --- | --- |
| `profile`, `project`, `kind` | Selected profile name, manifest project name, and project kind |
| `backend` | Effective project backend after profile and CLI selection |
| `compiler`, `assembler`, `linker` | Selected stage names for hosted C11 toolchains, or `nil` when no stage is selected |
| `directory` | Project-relative source directory, or `"."` |
| `target` | Name explicitly selected by `--target`, or `nil` |
| `targets` | Map from selected target names to project-relative entry paths; empty for a library |

Mutating a returned table (including `targets`) does not affect the build or
future snapshots. A profile runs once per invocation, not once per target.
Look up targets by name rather than relying on table iteration order. There
is no host path or target probing in this API. Existing dependency and
optimization lock checks still apply; the root manifest and selected script
are recorded in build/plan input digests, not pinned by the lockfile.

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
otherwise give `--mune-runner` and `--qmr-compiler` from the matching release.
Outside sandbox mode `--libstone-root DIR` can compile the actual QMR compiler
helper on demand.
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
Declared ref names use a conservative ASCII subset and cannot begin with an
option, contain traversal/dot-lock syntax, or use Git revision expressions.
An unrelated non-ASCII ref in the local store does not affect resolution:
packing the refs leaves the lock for the same branch or tag unchanged.

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

The dependency-only lockfile has schema `on1x.lock/v1`; a v2 lock also pins
the [compiler policy](#compiler-optimizations). Each dependency entry records the dependency kind,
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

Default project builds stage declared inputs and verify the module graph.
Build subprocesses get a fixed environment, including `PATH=/usr/bin:/bin`,
not the caller's `PATH`, `CC`, compiler include settings, or library search
variables. The driver still accepts operator tool selections such as
`--on1xc` and the SDK's `ON1X_PROJECT_COMPILER`. Bare executable names must be
found on the fixed `PATH`; for a tool beside the caller, use an explicit
`./tool` or absolute path. Tools receive separate arguments, without implicit
shell interpretation. The default does **not** OS-isolate the compiler or host
tools. Builds do not fetch dependencies; ordinary source PPX and executable
source extensions are rejected. `--frozen` enforces offline, locked
dependency resolution; it does not imply `--sandbox`, pin a toolchain, or
freeze the bytes of local sources. Use both flags to request both behaviors.

`--sandbox` checks Linux bubblewrap before starting even the stdlib query.
A setup or command failure never retries the build on the host. Compiler
invocations (including the stdlib query), the native builder, and selected
policy helpers get the isolated network namespace, private `/tmp`, `/proc`,
and `/dev`, and a fixed working layout: read-only `/work/input`, writable
`/work/cwd` and `/work/artifact`. The private host staging directory is not
mounted wholesale. The host driver still performs filesystem acquisition,
offline Git validation, the ArchVirt file-list query described above, and
output publication outside the sandbox.

The SDK, selected tools and library roots, and host `/usr` toolchain are
trusted and visible in the sandbox. The project and home directory are not
mounted merely because they are the project or home; files already under a
mounted tree remain visible. Keep private and undeclared project data outside
these trees. If a compiler wrapper delegates to a binary that is not visible
inside the sandbox, select the binary directly or install it in the mounted
toolchain. `tools.bubblewrap` in a sandboxed build report fingerprints the
launcher but does not pin it; reports are not a complete toolchain lock.
Artifact equality across hosts or toolchain changes is not guaranteed. See
[reproducible debug paths](../../docs/reproducible-debug.md) for path remapping.
Tool correctness, concurrent local changes to trusted mounts, and execution
after linking are outside the build sandbox guarantee.
