# Source-backed foreign contracts

`on1x/contracts-exoml` implements an **annotation and verification workflow**, not a
general safe FFI. Its ExoML v2 sidecars cover every core declaration in these
profiles:

| Profile | Functions | Important obligations |
|---|---:|---|
| JSON | 20 | owned roots, borrowed children/keys/text, conditional insertion, destructive replacement, independent serialized text |
| Vec | 7 | initialized byte-stride storage, realloc invalidation, self-aliasing push, no nested element destructors |
| Parsec combinators | 18 | borrowed value descriptors, transitive referents, pure indirect predicates, capture ownership and failure cleanup |
| ElviScrape | 6 | bounded caller-owned buffers, conditional status outputs, policy intersection, verified TLS peer callbacks and unknown transport effects |

The separate [Exotract static binding workflow](../contracts-exotract/README.md)
uses this catalog's core implementation recipes and MANIFEST-backed profiles for
the remaining MuLIB modules. It produces typed import metadata and a compiled
static archive with one kernel implementation TU and any native companion objects.

All shipped contracts are **AI-authored, annotated and unreviewed**. Header and
manual SHA-256 pins identify what was audited; they do not certify behavior.
`obligations.json` records preconditions, branch semantics, field-level limits
and missing lowering. The optional owned `MULIB_IMPL_PARSER` grammar/AST API is
outside this Parsec profile.

## Inspect without LibStone

From the MuLIB root:

```sh
python3 on1x/tools/on1x-contracts.py lint
python3 on1x/tools/on1x-contracts.py build-plan json vec parsec
```

Lint checks the pinned canonical parent headers/manuals and annotation inventory;
it explicitly reports **`gate_b: false`**. The build plan consults `MANIFEST.yaml`
for real module names, headers, macros and dependencies, and combines the
manual-backed core recipes. Define each listed implementation switch in
**one C11 translation unit**, not once per includer. Optional Parsec dependencies
and its extra `PARSER` switch are recorded but never implicitly enabled.

The [C example](../examples/contracts/contract_usage.c) demonstrates actual kernel
ownership/cleanup. It is a C example, not an On1x safe-import facade.

## Real extraction and a reproducible snapshot

Build-time requirements: the user-supplied LibStone SDK; libclang development
headers/library and the SDK's OpenSSL/JSON extraction dependencies when discovery
is enabled. Generated applications do **not** link to these dependencies.

```sh
cmake -S on1x -B /tmp/opencode/on1x-contract-build \
  -DON1X_REQUIRE_LIBSTONE=ON \
  -DON1X_LIBSTONE_ROOT="$PWD/on1x/third_party/libstone" \
  -DON1X_WITH_EXOML=ON -DON1X_WITH_EXOML_EXTRACT=ON \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DBUILD_TESTING=ON
cmake --build /tmp/opencode/on1x-contract-build --parallel 4
python3 on1x/tools/on1x-contracts.py snapshot /tmp/opencode/contract-evidence \
  --bind /tmp/opencode/on1x-contract-build/on1x-bind \
  --resource-dir "$(clang -print-resource-dir)"
python3 on1x/tools/on1x-contracts.py check /tmp/opencode/contract-evidence \
  --bind /tmp/opencode/on1x-contract-build/on1x-bind
```

Choose a **new destination** each time. Snapshot generation calls only the
explicitly authorized `--bind` executable, without a shell. It extracts canonical
CDecl, verifies complete declaration coverage, pins the full sidecar, runs Gate B
and writes a locked directory containing:

- `NAME.cdecl.json`: compiler/target/options, types/layouts, input hashes and
  preprocessing evidence from the public ExoML API;
- `NAME.exocontract.json`: full source annotations pinned to that unit's identity
  and declaration locations, still **unreviewed**;
- `NAME.check.json`: Gate B, separate review-metadata status, raw scalar
  eligibility/reasons, and an unconditional `safe_export: false`;
- `build-plan.json` and `lock.json`: implementation recipe and hashes of every
  artifact, discovery input, annotation, header/manual/manifest, binding tool,
  Python driver/executable and Python version.

Publication uses Linux `renameat2(RENAME_NOREPLACE)` so it cannot replace an
existing directory, even an empty one. Recheck hashes all recorded inputs and
artifacts, reconstructs the pin from current annotations, reruns Gate B and
compares the plan/report. It never executes a tool path obtained from a lock.
`--library vec` selects one profile; `--target TRIPLE` records cross-target
discovery. This does not prove that cross-target executables compile or run.

Snapshots intentionally bind to absolute source/toolchain paths. Regenerate for
a different checkout, compiler, options, resource directory, header, manual or
manifest. After intended kernel changes, **audit the semantics first**, then
refresh annotations and catalog hashes together. There is no auto-review or
automatic hash refresh. Hashes detect accidental drift, not an adversary who
rewrites both inputs and evidence; this is not a signed attestation or sandbox.

## `on1x-bind` commands

| Command | Behavior |
|---|---|
| `extract HEADER --revision REV ...` | real `cdecl::extract_c11`; complete discovered artifacts only |
| `normalize-decl DECL` | lossless canonical public-CDecl transport check |
| `pin DECL CONTRACT` | stamp exact identity/locations, reset to annotated/unreviewed |
| `check DECL CONTRACT` | strict shape/identity/index checks plus ExoML Gate B; review not required |
| `explain DECL CONTRACT` | same machine-readable report with per-function unsupported reasons |
| `verify DECL CONTRACT` | Gate B plus reviewed-state metadata, explicitly not a semantic proof |
| `emit-raw DECL CONTRACT PREFIX` | reviewed-metadata-only narrow raw C11 adapters, On1x signatures and provenance manifest |
| `emit-safe DECL CONTRACT` | fail closed: remaining safe-import gates are not implemented |

`emit` is the compatibility alias for `emit-raw`. Contract/discovery outputs are
new files only; no existing file, symlink or hard link is overwritten. Pin and
normalization print to stdout. JSON reports go to stdout, diagnostics to stderr;
exit 2 denotes malformed input/usage and 3 a failed admission/unsupported gate.

Extraction accepts repeated `--include`, `--define`, `--undefine`,
`--provenance`, `--clang-arg`, plus `--target`, `--resource-dir`,
`--compile-commands`, `--compile-file` and `-o`. Revision is mandatory. Compiler
arguments/databases use ExoML's restricted ingestion, **never** shell execution.
With `ON1X_WITH_EXOML=ON` but discovery disabled, every artifact command still
works; `extract` reports unsupported instead of pretending to inspect a header.

The supplied SDK revision's CDecl JSON reader reconstructs only part of its
model. On1x restores all wire fields into **public** `cdecl::TranslationUnit`
structs and requires a lossless canonical serializer round trip, including
types, preprocessing, diagnostics and compilation context. Contract admission
also rejects unknown nested fields, duplicate JSON keys, wrong types, NUL,
invalid UTF-8/surrogates, invalid indices and incomplete provenance before
calling the public validator. No private SDK header or parallel C type system
is used; LibStone is not patched.

## ABI, ownership and honest boundaries

Raw emission supports only nonvariadic C prototypes returning signed integer or
void, with signed integer parameters. It rejects pointers, unsigned/enum/floating
types, aggregates, callbacks, unknown effects, unsupported conventions and
ownership/error/invalidation obligations. Inline scalar APIs can be included
directly. Adapters use `_Generic` on the **actual function pointer**, C11 static
size/signedness assertions and checked i64 narrowing. Full IO, callback,
ownership and concurrency information remains in the manifest; raw externs do
not become purity or lifetime proofs.

All 51 shipped kernel functions are intentionally ineligible for this scalar
emitter, because their true ABIs/contracts require richer lowering. In
particular, Parsec's borrow-bearing structs are not guessed to be ordinary
scalars, and its descriptor-retained callback is not assigned a fictitious
unregister API. Indirect predicates keep unknown effects.

The optional [Chisel ownership CFG integration](memory-safety.md) now checks
scalar/JSON HIR usage and exposes a library-independent resource protocol checker.
It does not yet translate these ExoML sidecars into general foreign-handle ABI
adapters. No `Mulib.Safe.*`, `Owned`, `Option`/`Result` wrappers, Wetstone effect
proof or generalized foreign callback ABI is claimed. A reviewed flag alone
cannot unlock `emit-safe`. The JSON `or_die` pilot remains a trusted handwritten
facade whose client lifetimes can additionally be checked by Chisel.

## Bounds and tests

Native tools limit CDecl input to 64 MiB, sidecars to 2 MiB, JSON to 64 levels
and 1,048,576 nodes, arguments to 4,096 entries/bytes each, types/declarations to
16,384, contract functions to 4,096 and parameters to 128. SDK discovery has its
own work/input limits. Raw C names, type spellings and header basenames are
limited to 256 bytes and scanned iteratively, not through recursive regexes.
Python tooling uses bounded reads, 90-second child
timeouts and temporary private publication directories; it is developer tooling,
not hostile-process isolation. File-writing binding commands require POSIX;
snapshot publication currently requires Linux.

CTest registers always-available `on1x.contracts` semantics/source tests and
optional `on1x.contracts_sdk` transport/ABI/discovery tests. Counted allocator
failure loops check partial JSON trees, serialization and Parsec captures; Vec
tests check failed growth, internal aliases, logical removal and resource-free
byte copying. SDK tests deliberately assign review flags to **test fixtures**
only to test gates; they do not review the real annotations. See
[verification](../VERIFICATION.md) for executed configurations and the current
Clang/libstdc++ dependency-build limitation.
