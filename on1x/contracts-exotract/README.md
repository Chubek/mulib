# MuLIB static bindings through Exotract

This directory contains the MuLIB build-profile catalog and executable C clients
for `tools/on1x-exotract.py`. It uses the real LibStone **Exotract** CLI and linked
ExoML/libclang declaration extractor to produce:

- Compiler-derived CDecl v2 files and Exotract On1x import manifests.
- A declaration-only `mulib.h` including the selected canonical headers.
- One Exotract-generated `mulib.c` containing all implementation switches.
- `mulib.o`, optional native companion objects, and `libmulib.a`, compiled as C11.
- A build plan and an input/artifact SHA-256 lock file.
- `symbols.json`, separating actual archive exports from declaration-only APIs.

The binding source is `include/mulib-*.h`. `catalog.json` uses `MANIFEST.yaml`
as the module registry: **1,165 module profiles**, plus inventory reporting of
the three auxiliary headers `mulib-container-core.h`, `mulib-qvm.h` and
`mulib-lprowse.h`. Those helpers have no independent MANIFEST implementation
recipe; they remain included by their owning kernels. This is a library-wide
registry, not a claim that every module has been compiled on every platform.

## Build the generator

From the MuLIB root:

```sh
cmake -S on1x -B /tmp/opencode/on1x-exotract \
  -DON1X_WITH_EXOML=ON -DON1X_WITH_EXOML_EXTRACT=ON \
  -DON1X_WITH_EXOTRACT=ON -DBUILD_TESTING=ON
cmake --build /tmp/opencode/on1x-exotract --parallel 4
ctest --test-dir /tmp/opencode/on1x-exotract --output-on-failure
```

This needs the supplied LibStone checkout, libclang development files and ExoML's
normal extraction dependencies. The On1x option uses `Moonstone::Exotract`'s CLI
target with linked ExoML extraction; Lua, LMDB, ucpp and CParser are unnecessary
for this workflow. It does not fetch or alter the supplied dependency checkout.
The optional feature defaults OFF.

## List, plan, generate and link

```sh
python3 on1x/tools/on1x-exotract.py list
python3 on1x/tools/on1x-exotract.py plan json vec parsec
python3 on1x/tools/on1x-exotract.py build json vec parsec \
  --exotract /tmp/opencode/on1x-exotract/libstone-moonstone/exotract/exotract \
  --compiler clang --output /tmp/opencode/mulib-static
clang -std=c11 -Wall -Wextra -Werror \
  -Iinclude -I/tmp/opencode/mulib-static \
  on1x/contracts-exotract/examples/client.c \
  /tmp/opencode/mulib-static/libmulib.a -lm -o /tmp/opencode/static-client
/tmp/opencode/static-client
```

The client prints `[42]` and exercises Vec and Parsec as well as JSON. Select
`microjs` instead to build the MicroJS/MiniVM archive and link `examples/microjs.c`,
which evaluates JavaScript and prints `42`.

GCC is supported with `--compiler gcc --resource-dir "$(clang -print-resource-dir)"`.
Clang's resource directory is required by the independent libclang extractor;
it does not change the selected C compiler. Repeat `--cflag=FLAG` for ABI switches
accepted by Exotract, or joined `-DNAME=VALUE`/`-UNAME` options. Debug flags `-g`
through `-g3`, `-fsanitize=address,undefined`, `-fno-omit-frame-pointer` and
`-fno-sanitize-recover=all` apply only to archive code generation; discovery uses
the same ABI options without instrumentation. Both option sets are recorded.
Select an archiver with
`--ar`; it must support deterministic `rcsD` archives. Source relocation uses the
global `--root PATH` option before `list`, `plan` or `build`.

`--nm` selects the symbol inspector (default `nm`, GNU-compatible POSIX output).
The core Parsec header declares 25 functions, but its core archive exports 18;
the seven optional `mulib_parser_*` functions are recorded as declaration-only.
Always consult `symbols.json` when selecting archive entry points. Header-only
inline functions may also be declaration-only in the archive symbol inventory.

ElviScrape, JSON, Vec and core Parsec reuse the source-pinned implementation recipes in
[`../contracts-exoml/catalog.json`](../contracts-exoml/catalog.json). This avoids
silently enabling Parsec's separate owned grammar API. Other module recipes
follow MANIFEST dependency closure, recording cycles and rejecting unknown modules. Multiple
selected modules share one deduplicated implementation TU. The generated header
does not define implementation macros. **Do not combine this archive with a
second TU defining the same MuLIB implementations.**

The shared [`std.mulib` planner](../std/mulib/README.md) also matches native
companions and resource fixtures under `library/`. `--fixture yantlr` opts into
its provider header, implementation and typed Exotract metadata; selecting
Shiraz includes its separate runtime/registry objects and KVDB kernels.
Copied fixtures and implementation-only include files participate in the lock.
Use `on1x-build.py --mulib-archive DIRECTORY` for verified archive consumption.

The plan includes the profiles' and fixture registry's declared link requirements
(`m` for JSON and MicroJS, for example). For other modules, consult their manuals for final
platform/system-library link flags. Compilation failures and missing configured
features are reported; the tool never substitutes declarations for a failed
implementation build.

## On1x executable use

Exotract's On1x binder emits **typed metadata**, not executable On1x declarations.
Its import manifests retain the complete CDecl graph, `unsafe_by_default:true`
and per-import `safe:false`. Arbitrary pointers/records/callbacks still need
compiler ABI lowering and semantic ownership contracts. ExoML annotations remain
in `contracts-exoml`; extraction does not promote their review state.

The existing On1x JSON facade can already link the generated archive:

```sh
/tmp/opencode/on1x-exotract/on1xc --emit-c on1x/examples/json.on1x \
  -o /tmp/opencode/json-client.c
clang -std=c11 -Ion1x/runtime -Iinclude /tmp/opencode/json-client.c \
  on1x/runtime/on1x_runtime.c on1x/runtime/on1x_json.c \
  /tmp/opencode/mulib-static/libmulib.a -lm -o /tmp/opencode/json-client
/tmp/opencode/json-client
```

This runs real MuLIB through the established handwritten facade and prints the
JSON example's three-element result. Ordinary clients link no Exotract, ExoML,
libclang or C++ compiler support library.

## Publication and evidence

Discovery runs with implementation macros disabled and requires complete typed
declarations plus C11 syntax validation. Core profiles additionally require known
symbols. Implementation macros are emitted only by Exotract into the kernel C TU;
passing them via `--cflag` rejects. Compiler options are explicit trusted build
inputs, not a restricted execution sandbox.

The output directory must be new. All discovery, compilation and archiving happen
in a sibling staging directory; failure removes staging and leaves existing
outputs intact. Publication uses the existing Linux atomic no-replace directory
helper. The lock records compiler identity, flags, resource directory, tool/input
hashes and artifact hashes. Source fingerprints are rechecked before publication.
The lock is evidence, not a safe-import certificate or an executable instruction
file. Generated artifacts belong in build directories rather than this source
catalog.

`on1x.exotract` tests real extraction, complete metadata, one kernel archive member,
separate-TU C and On1x execution, MicroJS, YANTLR provider discovery/execution,
Shiraz's multi-object archive, fixture drift, mismatched compiler options, unknown
modules, missing declarations, implementation-macro rejection and rollback.
Existing `on1x.contracts` and
`on1x.contracts_sdk` tests exercise the renamed ExoML workflow. Measured builds
and dependency revisions are in [`../VERIFICATION.md`](../VERIFICATION.md).

The standard-library bridge test also extracts/builds JSON/YAML/TOML/INI,
cryptohash and socket kernels into a shared archive and executes an On1x consumer
through `--mulib-archive`, with no manually specified bridge TU or duplicate
implementation switches. This verifies raw linkage, not general foreign safety.

`plan elvi-scrape` and `build elvi-scrape` use the pinned complete one-TU
recipe and require all six exported symbols. `examples/elvi-scrape.c` executes
offline provider compilation, URL expansion and extraction against that archive.
TLS session fixtures test the adapter independently; typed imports remain unsafe.
