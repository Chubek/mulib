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

The binding source is `kernels/mulib-*.h`. Both this catalog and the
[ExoML inventory](../contracts-exoml/README.md) use `MANIFEST.yaml` as the
module registry. `list` reports each canonical header, manual, implementation
switch and dependencies, plus its archive recipe and whether any semantic
sidecar or core snapshot profile exists. All 2,053 public headers are registered,
including the static-inline `container-core` and `qvm` helpers and the
discovery-only static-data `lprowse` module. Registration
does not mean a kernel builds on every platform or has a callable KernelBind
profile. The [generated KernelBind catalog](../docs/kernel-bind.md#refresh-automatic-discovery)
adds declaration discovery without changing these archive recipes.

The `cdd-*` modules are **discovery-only**. `plan cdd-resolve` reports its
MANIFEST closure, labeled `discovery_only`; `build` refuses to generate a CDD
archive. The [KernelBind inspector](../docs/kernel-bind.md#inspect-requests-and-discover-declarations)
can extract declaration metadata for `mulib.cdd_*`. It neither attaches a
semantic sidecar nor enables callable bindings.

The `workspace-fs`, `unidiff`, `workspace-edit` and `process-stream` kernels
can be planned and built together as a raw C11 archive on Linux. `pty` supplies
the internal-linkage process dependency and has no implementation switch.
None of the four kernels has a reviewed On1x callable profile; discovery and
the raw archive do not authorize workspace access or process launch.

Bounded static-inline entries, including the 193 media and compute kernels,
are registered by the same MANIFEST inventory. Their source import aliases
replace hyphens with underscores (`mulib.img_blend`, `mulib.media_timebase`).
`--inspect-kernels` extracts their actual internal-linkage definitions into
CDecl v2. `plan` labels them discovery-only and ordinary `build` rejects them.
The separate, explicitly selected raw C11 workflow below can generate new
external forwarding symbols for a bounded subset. It does not change discovery,
KernelBind lowering, semantic contract coverage, or review status.

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

## Explicit raw C11 wrappers for static-inline functions

Configure the build above with `-DON1X_WITH_EXOTRACT_LUA=ON` to run the pinned
ExoLib recipe. Select each symbol by its MANIFEST module and exact C name
(not an import alias):

```sh
python3 on1x/tools/on1x-exotract.py wrap-inline \
  media-timebase:mulib_media_timebase_rescale \
  media-timebase:mulib_media_timebase_gcd media-core:mulib_media_i64_add \
  --exotract /tmp/opencode/on1x-exotract/libstone-moonstone/exotract/exotract \
  --compiler gcc --resource-dir "$(clang -print-resource-dir)" \
  --output /tmp/opencode/media-timebase-raw
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -Ikernels -I/tmp/opencode/media-timebase-raw \
  -c on1x/tests/fixtures/inline_wrappers_client.c -o /tmp/opencode/time-client.o
gcc /tmp/opencode/time-client.o /tmp/opencode/media-timebase-raw/libmulib.a \
  -o /tmp/opencode/time-client
/tmp/opencode/time-client
```

`on1x-inline.h` declares the external symbols `on1x_inline_v1_<original-name>`;
`mulib.c` implements them by directly forwarding to the still-internal kernel
definitions. There is no conversion, pointer validation, ownership transfer,
status adaptation or safety promotion. Use the exact pinned headers and target
when compiling a caller. For example, `rescale` accepts two canonical rational
records by value (seconds per tick) and forwards an `int64_t *` for the result.
A nonnull output pointer must designate a live, aligned, writable `int64_t`;
the wrapper preserves the kernel's rounding toward zero and failure behavior.

Native Linux x86-64 GNU C11 only. The bounded graph projection accepts
nonvariadic default-C prototypes with supported integer and floating scalars,
flat named padding-free scalar structs, and one level of pointer parameters to
supported values. It uses CDecl type IDs, not a parser for C source tokens.
`_Generic` checks compatible function and field types; `_Static_assert` checks
size, alignment and field offsets in both the producer and any C11 consumer
including the generated header. Enums, unions, bitfields, arrays, nested or
padded aggregates, opaque/`void` pointees, function pointers, pointer returns,
nondefault calling conventions, and unsupported qualifiers or attributes are
outside the profile. Only the documented optimization/debug/sanitizer flags
from KernelBind are accepted; user macro and ABI flags are not.

The existing MANIFEST planner deduplicates implementation switches across
selected modules. The tool checks the archive has one `mulib.o` member and
links references from a separate translation unit before publication. The
lock pins the supplied compiler/tools, inputs, CDecl fingerprints and wrapper
symbol map. Selection order does not affect generated sources or plans for
fixed inputs; debug object bytes are not promised reproducible across builds.
The lock schema `on1x.exotract-raw-inline/v1` is audit evidence only; it cannot
be used as an ordinary `--mulib-archive` lock, a KernelBind compiler input or a
signed review. Neither KernelBind import spelling becomes callable.

Run the executable regression with
`python3 on1x/tests/test_inline_wrappers_sdk.py EXOTRACT ON1XC on1x WORKDIR gcc`.
It includes a separate-TU C11 client, wrong member/prototype and duplicate-TU
rejections, and confirmation that safe and unsafe KernelBind imports still fail.

## List, plan, generate and link

```sh
python3 on1x/tools/on1x-exotract.py list
python3 on1x/tools/on1x-exotract.py plan json vec parsec
python3 on1x/tools/on1x-exotract.py build json vec parsec \
  --exotract /tmp/opencode/on1x-exotract/libstone-moonstone/exotract/exotract \
  --compiler clang --output /tmp/opencode/mulib-static
clang -std=c11 -Wall -Wextra -Werror \
  -Ikernels -I/tmp/opencode/mulib-static \
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

Mune, ElviScrape, QUIC, HTTP/3, JSON, Vec and core Parsec reuse the source-pinned implementation recipes in
[`../contracts-exoml/catalog.json`](../contracts-exoml/catalog.json). This avoids
silently enabling Parsec's separate owned grammar API. Other module recipes
follow MANIFEST dependency closure, recording cycles and rejecting unknown modules. Multiple
selected modules share one deduplicated implementation TU. The generated header
does not define implementation macros. **Do not combine this archive with a
second TU defining the same MuLIB implementations.**

`plan http3 quic` includes the opt-in native client switches and TLS/UDP
closure; `build http3 quic` exports all 20 declarations in the raw archive.
There is no external QUIC/TLS runtime dependency. For a separate C11 consumer,
see [`examples/http3.c`](examples/http3.c). Source imports inspect the complete
CDecl and unreviewed sidecars, but do not produce callable On1x bindings.

Mune's full recipe explicitly enables `MULIB_IMPL_PARSER` and requires all 52
public Mune symbols to be exported, with no declaration-only fallback. It pins
the updated header/manual and the runtime, allocation and differential test
sources in its ExoML profile. Those five source/evidence files participate in
declaration provenance and the archive lock; drift invalidates rebinding.

```sh
python3 on1x/tools/on1x-exotract.py build mune \
  --exotract /tmp/opencode/on1x-exotract/libstone-moonstone/exotract/exotract \
  --compiler clang --output /tmp/opencode/mune-static
clang -std=c11 -Wall -Wextra -Wpedantic -Werror -Ikernels \
  on1x/contracts-exotract/examples/mune.c /tmp/opencode/mune-static/libmulib.a \
  -pthread -lm -ldl -o /tmp/opencode/mune-client
/tmp/opencode/mune-client
```

This separate caller TU prints `mune: archive OK` after exercising actual Lua
libraries, same-state/GC rooting, binary strings, native callbacks, retained
chunks, error-output preservation and module bundles in tree/VM/JIT modes.
The matching On1x acceptance client uses an ABI-exact raw `int64_t` test adapter;
it does not synthesize safe pointer/union/callback On1x declarations.

The shared [`std.mulib` planner](../stdlib/mulib/README.md) also matches native
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
clang -std=c11 -Ion1x/runtime -Ikernels /tmp/opencode/json-client.c \
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
Shiraz's multi-object archive, Mune's complete C and On1x archive consumers,
fixture drift, mismatched compiler options, unknown
modules, missing declarations, implementation-macro rejection and rollback.
Existing `on1x.contracts` and
`on1x.contracts_sdk` tests exercise the renamed ExoML workflow. Measured builds
and dependency revisions are in [`../VERIFICATION.md`](../VERIFICATION.md).

The standard-library bridge test also extracts/builds JSON/YAML/TOML/INI,
cryptohash and socket kernels into a shared archive and executes an On1x consumer
through `--mulib-archive`, with no manually specified bridge TU or duplicate
implementation switches. This verifies raw linkage, not general foreign safety.

`plan elvi-scrape` and `build elvi-scrape` use the pinned complete one-TU
recipe and require the six legacy entry points plus shared lexical validation
and TLS connector symbols. `examples/elvi-scrape.c` executes
offline provider compilation, URL expansion and extraction against that archive.
The archive includes the pinned legacy search profiles and the legacy
Google and Bing fixture paths. The programmable ElviScript execution
subsystems are not admitted by this profile.
TLS session fixtures test the adapter independently; typed imports remain unsafe.

## Sandbox kernel profiles

See [Sandbox kernels: build and inspect, not import](../docs/kernel-sandbox.md)
for the native one-TU archive recipe, declaration evidence and required SDK.
The sandbox profiles do not generate callable On1x adapters.
