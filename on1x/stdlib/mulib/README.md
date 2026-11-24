# `std.mulib`: native interoperability housekeeping

This package connects On1x programs, canonical `kernels/mulib-*.h` kernels,
and companion files under `library/`. It supplies a native build planner,
fixture registry, immutable runtime metadata, binary-safe fixture access and
host-size checks. Import it with `import std.mulib as M;` or
`from std import mulib;`.

## Build and use

This package now resides in the canonical `stdlib/mulib` tree; all build,
installation and extraction tools use that location. Kernel discovery accepts
colon-separated `ON1X_MULIB_PATH` or repeated `--mulib-path DIR:DIR`; selected
headers and hashes enter the shared single-TU plan. See
[search paths and manifests](../../docs/search-paths.md) for exact precedence,
standard-library/PPX lookup, source-pin checks and limitations.

```sh
python3 on1x/tools/on1x-mulib.py plan yantlr --fixture yantlr
python3 on1x/tools/on1x-build.py on1x/examples/mulib.on1x \
  --on1xc on1x/build/on1xc --mulib-fixture yantlr \
  --mulib-plan build/native-plan.json -o build/mulib-example
build/mulib-example
```

```on1x
import std.mulib as M;
fn main() {
  assert(M.fits_array(1024,8));
  assert(M.has_kernel("yantlr"));
  let (found,text)=M.fixture_text("library/yantlr/example.y1");
  assert(found);
  io_stdout(text);
}
```

The build helper embeds selected fixture bytes and automatically links the
housekeeping adapter when its externs are used. Generated programs need neither
the source checkout nor fixture files at runtime. Returned strings borrow
immutable process-lifetime storage; no allocation or explicit release is needed.
Importing this package alone provides an empty inventory and host-size utilities
without requiring a MuLIB checkout.

## Runtime API

| Function | Result / behavior |
|---|---|
| `abi_version()` | Housekeeping ABI version, currently 1 |
| `pointer_bits()` | Actual target C pointer width |
| `max_size()` | Largest nonnegative On1x i64 representable as host `size_t` |
| `fits_size(n)` | Whether `n` is a valid nonnegative host size |
| `fits_array(count, element_bytes)` | Validates operands and multiplication bounds without multiplying |
| `kernel_count()`, `kernel_name(index)` | Enumerate the selected implementation closure |
| `has_kernel(name)` | Membership in that closure |
| `kernel_header(name)`, `kernel_sha256(name)` | Canonical relative header path and build-input fingerprint |
| `fixture_count()`, `fixture_name(index)` | Enumerate selected files in path order |
| `has_fixture(path)` | Exact repository-relative path lookup |
| `fixture_group(path)`, `fixture_sha256(path)` | Owning group and original-byte SHA-256 |
| `fixture_size(path)` | Byte count, or `-1` when absent |
| `fixture_byte(path,index)` | Unsigned byte as i64, or `-1` for absent/out-of-bounds |
| `fixture_text(path)` | `(bool,string)`; false for absent files or embedded NUL |
| `require_fixture(path)` | NUL-free text, or the ordinary fatal tool diagnostic |

Unknown names and invalid enumeration indexes yield empty metadata strings.
Empty text files are successful lookups. Binary files retain every byte,
including NUL; use `fixture_byte`. Zero-sized elements are allowed by `fits_array`,
but negative/unrepresentable operands reject. Size checks do not establish pointer
validity, allocation success, ownership or ABI layout.

The exact scalar C ABI is in [`runtime.h`](runtime.h); source declarations are
in [`Native.on1x`](Native.on1x). C adapters can read the same tables. General
pointer/record/callback binding remains part of On1x's ExoML/type/ownership
pipeline; existing `extern fn` adapters remain raw.

## Kernel/fixture matching

[`catalog.json`](catalog.json) explicitly associates every current `library/`
directory with its owning kernels. Headers, manuals, dependency edges and
implementation switches come from `MANIFEST.yaml`, including `extra_macros`.

- **Runtime/profiles:** Git, ImmGTK, daemon/distributed profiles, Readline,
  ArchML/DSymSpec, OptML, MicroChan and ElviScrape are selected automatically for
  their owners. `--mulib-fixture elvis` selects the ElviScrape kernel and embeds
  the providers in `library/elvis/` along with the legacy Google and Bing paths.
  These files are compiled offline as declarative data, never executed as code.
- **Embedded/amalgamated sources:** lmuw3, lprowse, ldocstr, lcharts, lpaleo and
  Virtio retain source fixtures for provenance. Canonical generator `--check`
  commands verify their checked-in kernel representations. Standalone
  implementation-placement files are inventoried, not compiled a second time.
- **Native companions:** Shiraz's `runtime.c`, `kernel32.c`, and `registry.c`
  compile separately, with KVDB added. Its `core.c` is not compiled: those kernel
  switches belong to the generated implementation TU.
- **Opt-in provider:** `--mulib-fixture yantlr` selects the actual provider header,
  implementation switch and example in `library/yantlr`. Selecting the kernel
  alone retains its explicit provider opt-in semantics.
- **Test data:** WebIDL and statistical/text vectors are inventoried and selected
  explicitly with `--mulib-fixture GROUP`.
- **Empty:** `immgtk-widgets` is recorded as empty and rejects explicit selection.

Repeat `--mulib KERNEL` and `--mulib-fixture GROUP` as needed. A fixture selects
its primary kernel, not every associated consumer. Generic dependency cycles
are recorded and deduplicated: Parsec/PEG genuinely depend on each other and
share one implementation TU. All switches precede inclusion. Direct requests
for JSON/Vec/core-Parsec retain their narrower source-pinned ExoML recipes.
Explicit fixture selection also retains a primary kernel's source-pinned recipe,
required exports and evidence obligations. ElviScrape enables only its six-API
profile and full HTTPS dependency closure, leaves the optional owned parser off,
and records `bounded-text`'s C math-library link requirement.
Mune also uses a source-pinned recipe, explicitly activating the owned parser's
`MULIB_IMPL_PARSER` switch and checking all 52 public exports. Its header, manual
and three Lua-runtime regression/evidence files are locked build inputs; this
does not broaden the standalone Parsec profile or confer safe-import status.

Known link libraries and native companions are explicit registry entries. A
recipe does not establish that every kernel/platform has been executed; missing
platform APIs or additional requirements are reported by compilation/linking.
Measured configurations are in [`../../VERIFICATION.md`](../../VERIFICATION.md).

## Native builds and evidence

```sh
python3 on1x/tools/on1x-build.py client.on1x --on1xc on1x/build/on1xc \
  --mulib shiraz --extra-c adapter.c --mulib-plan build/link.json -o build/client
```

- `--mulib-root DIRECTORY` or `ON1X_MULIB_ROOT` selects the canonical checkout,
  including when using an installed helper with an external MuLIB source tree.
- `--extra-c` supplies caller-authored ABI-exact adapters. Duplicate implementation
  switches in supplied C files or compiler options reject.
- The existing JSON facade joins the same implementation plan, preventing a
  second JSON implementation when another selected kernel depends on it.
- `--mulib-plan FILE` records the recipe, compiler-discovered include dependencies,
  generated-source hashes and executable hash. GCC/Clang `-MM` captures
  implementation-only `.inc` files and companion headers. Inputs are fingerprinted
  before compilation and rechecked before publishing output.
- Compilation/linking use temporary output. Failed linking preserves an existing
  executable.

Typed extraction and archives share this planner:

```sh
python3 on1x/tools/on1x-exotract.py build yantlr --fixture yantlr \
  --exotract build/on1x/libstone-moonstone/exotract/exotract \
  --output build/yantlr-archive
python3 on1x/tools/on1x-build.py client.on1x --on1xc on1x/build/on1xc \
  --mulib-archive build/yantlr-archive --extra-c adapter.c -o build/client
```

Exotract emits real CDecl/import metadata for requested kernels and selected
provider headers. Archives contain native companion objects where required and
exactly one kernel implementation object. Locks include copied fixtures and
compiler-discovered implementation inputs. Consumption checks artifact/source
fingerprints and required implementation coverage before linking.
The consumer must supply the archive's recorded `--cflag` list and compiler
target; changed ABI options such as `-fshort-enums` require rebuilding the archive.
`safe_export` remains false: housekeeping is not foreign-contract review.

## Inventory, audit and filesystem bundles

```sh
python3 on1x/tools/on1x-mulib.py list
python3 on1x/tools/on1x-mulib.py audit --all
python3 on1x/tools/on1x-mulib.py bundle --fixture librl --output build/resources
python3 on1x/tools/on1x-mulib.py verify build/resources
python3 on1x/tools/on1x-mulib.py verify build/resources --sources
```

Bundles preserve `library/GROUP/...` paths for hosts that need files. Move a
bundle as a unit: ordinary verification uses its copied artifacts; `--sources`
also checks original checkout/registry identities. Atomic no-replace publication
uses the existing Linux helper. Auditing never regenerates kernels, executes
fixtures, launches daemons or fetches external resources.

Paths must remain inside their selected root after symlink resolution. Missing
glob matches reject. Limits: 8,192 fixture files and 64 MiB of fixture bytes per
selection, 4,096 kernels and 128 active dependency levels. Generator checks have
a 120-second timeout. Hashes detect drift, not maliciously rewritten evidence.

## Tests

`on1x.mulib` executes YANTLR, OptML, Shiraz, embedded lmuw3 and shared JSON, then checks binary
fixtures, size bounds, executable relocation, bundles, drift, missing/escaping
files and rollback. `on1x.install` runs the installed namespace and builds a native
fixture consumer with an external MuLIB root after prefix relocation.
`on1x.exotract` tests real typed provider discovery, multi-object companion
archives, archive-backed On1x execution and tamper rejection.
