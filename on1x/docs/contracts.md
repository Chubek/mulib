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
| Mune | 52 | legacy/state/chunk/result ownership, same-state GC roots, binary strings, retained coroutine-thread callbacks, AOT entry lifetimes and arbitrary Lua/OS effects |

The separate [Exotract static binding workflow](../contracts-exotract/README.md)
uses this catalog's core implementation recipes and MANIFEST-backed profiles for
the remaining MuLIB modules. It produces typed import metadata and a compiled
static archive with one kernel implementation TU and any native companion objects.

All shipped contracts are **AI-authored, annotated and unreviewed**. Header and
manual SHA-256 pins identify what was audited; they do not certify behavior.
`obligations.json` records preconditions, branch semantics, field-level limits
and missing lowering. The optional owned `MULIB_IMPL_PARSER` grammar/AST API is
outside this Parsec profile.
Mune explicitly opts into that owned parser through `active_optional_macros`;
combined plans deduplicate the switch and do not report it as inactive. Its
`evidence_sha256` inventory also pins `tests/test_mune_runtime.c`,
`tests/test_mune_runtime_alloc.c` and `tools/check_mune_runtime.py`, binding all
five changed kernel/manual/test/tool files without treating test evidence as review.

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
and its extra `PARSER` switch are recorded but never implicitly enabled;
Mune's complete runtime recipe activates the switch explicitly.

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
| `emit-safe DECL CONTRACT PREFIX --on1xc PATH --cc PATH --workdir DIR [--cflag ARG] [--impl FILE]` | admit the reviewed scalar-only profile only after review-metadata, profile, Chisel-use and executable target gates pass |

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

Raw emission supports nonvariadic C prototypes over signed/unsigned integers,
C `_Bool`, binary32 `float` and binary64 `double` (including typedefs), with
`void` allowed only as a result.
It rejects pointers, enums, `long double`/other floating formats, aggregates,
callbacks, unknown effects, unsupported conventions and
ownership/error/invalidation obligations. Inline scalar APIs can be included
directly. Adapters use `_Generic` on the **actual function pointer**, C11 static
size/alignment assertions, exact `_Bool`/`float`/`double` type checks, floating format
checks, integer range/signedness checks and checked i64 narrowing. Unsigned
adapters use exact `uint8_t/uint16_t/uint32_t/uint64_t` carriers, not signed i64;
binary32 adapters use actual C `float`, not double. Floating adapters
match the runtime's Linux x86_64 GCC/Clang/SSE2, `FLT_EVAL_METHOD == 0` profile
and reject exposed fast/finite-only/relaxed IEEE math modes. These checks do not
verify the foreign implementation's arithmetic or floating environment behavior.
Full IO, callback,
ownership and concurrency information remains in the manifest; raw externs do
not become purity or lifetime proofs.

### Reviewed scalar-only safe admission

**M2 continuation:** the additional
[`reviewed-immutable-string/v4` profile](ffi-strings.md) admits exact immutable
call-scoped `const char *` inputs and non-null immutable static results through
the same four gates. It does not promote shipped bridge contracts to reviewed,
add owned buffers or certify foreign C bodies. The v1/v2/v3 scalar profiles
described below are unchanged.

`emit-safe` implements deliberately narrow versioned profiles. Signed-integer-only
bundles retain `reviewed-scalar/v1`; bundles adding `_Bool` or binary64 `double`
select `reviewed-scalar/v2`; any unsigned integer or binary32 `float` selects v3.
Their On1x parameters/results are exactly `i64`, `u8/u16/u32/u64`, `bool`, `f32`
or `f64`, never approximate integer encodings of floats or booleans.
It accepts a bundle only when **every** function has an exact
nonvariadic admitted scalar ABI, scalar parameters, value return, known
effects, no error classification, and no pointer, aggregate, callback,
ownership-transfer, destructor, lifetime or invalidation obligation. Effects
that require ownership or concurrency lowering (`allocate`, `free`, `escape`,
`thread`, `suspend`) reject. Declared `read`, `write`, `io` and `foreign_call`
effects remain visible in evidence; “scalar-only” does not mean pure.

Admission runs four gates rather than trusting a reviewed flag alone:

1. ExoML bundle/function provenance must record reviewed or verified status.
   These fields are accountable claims, not proofs of the C body.
2. The complete bundle must fit the scalar-only ownership/effect profile; safe
   emission is all-or-nothing, never a silently skipped subset.
3. The supplied real `on1xc` runs `--check-memory` over a generated On1x caller
   and the generated `on1x.admitted-extern/v1`, `/v2` or `/v3` companion. Chisel continues to
   reject every raw extern not named by matching admission evidence.
4. The supplied C compiler builds the adapter and boundary harness with strict
   C11 warnings, supplied implementation TUs and caller flags, then the bounded
   harness executes one representative call per function. Observed values are
   recorded but are **not** interpreted as semantic proofs.

The evidence wire format is versioned independently from the manifest's JSON
shape (`on1x.safe-ffi/v1`). v1 continues to admit only `i64` parameters and
`i64`/`void` results; v2 adds `bool` and `f64`; v3 adds `u8/u16/u32/u64/f32`.
Older evidence schemas never widen implicitly. Older compilers reject an unknown
schema rather than interpreting a new signature as an integer call. Existing signed-integer-only
modules need no source or evidence migration. New typed-scalar modules require
a compiler with the corresponding admission version and freshly executed gates.

Successful publication creates `PREFIX.on1x` (ordinary `on1x_safe_*` facade
functions over private raw declarations whose symbols include the complete
CDecl fingerprint), `PREFIX.c`,
`PREFIX.admitted-extern.txt`, and `PREFIX.manifest.json` with target/header/input
identity, contract provenance, effects, compiler flags, implementation paths,
Chisel report and runtime observations. Consumers requesting Chisel checks pass
the companion with `on1xc --memory-check --admitted-extern FILE ...`; omitting,
malforming, duplicating or drifting the evidence fails closed. Output files are
exclusive-create only and are preflighted before any gate runs. The gate
workdir must be new and is retained for audit.
`tools/on1x-build.py --memory-check --admitted-extern FILE --extra-c PREFIX.c`
forwards the same evidence while compiling the generated adapter as an ordinary
separate C translation unit.

This profile is safe admission at the On1x/C scalar boundary **conditional on
the reviewed semantic claims**. It does not verify arbitrary C implementation
bodies, provide signed attestations, or admit resources, pointers, buffers,
aggregates, errors, callbacks or thread-affine APIs. General safe FFI remains
open; `emit-raw` remains explicitly unsafe.

All 103 shipped kernel functions are intentionally ineligible for this scalar
emitter, because their true ABIs/contracts require richer lowering. In
particular, Parsec's borrow-bearing structs are not guessed to be ordinary
scalars, and its descriptor-retained callback is not assigned a fictitious
unregister API. Indirect predicates keep unknown effects.

The optional [Chisel ownership CFG integration](memory-safety.md) now checks
scalar/JSON HIR usage and exposes a library-independent resource protocol checker.
It admits the generated reviewed scalar-only evidence but does not yet translate
ExoML sidecars into general foreign-handle ABI adapters. No `Mulib.Safe.*`,
`Owned`, `Option`/`Result` wrappers, Wetstone effect proof or generalized foreign
callback ABI is claimed. A reviewed flag alone cannot unlock `emit-safe`. The
JSON `or_die` pilot remains a trusted handwritten facade whose client lifetimes
can additionally be checked by Chisel.

Mune's C aggregate/GC/callback interface is represented by exact, compiler-derived
CDecl and unreviewed field/lifetime obligations, not guessed On1x pointer handles.
The archive-backed C and raw-i64 On1x acceptance clients execute its legacy
environment, rooted results/userdata/native callbacks, bundle loader and Lua
standard-library regression cases. They do not establish a general safe Lua FFI.

## Bounds and tests

Native tools limit CDecl input to 64 MiB, sidecars to 2 MiB, JSON to 64 levels
and 1,048,576 nodes, arguments to 4,096 entries/bytes each, types/declarations to
16,384, contract functions to 4,096 and parameters to 128. SDK discovery has its
own work/input limits. Raw C names, type spellings and header basenames are
limited to 256 bytes and scanned iteratively, not through recursive regexes.
Python tooling uses bounded reads, 90-second child
timeouts and temporary private publication directories; it is developer tooling,
not hostile-process isolation. Safe gate stdout and stderr are each capped at 8 MiB;
compiler/build gates time out after 120 seconds and boundary execution after 30
seconds. Admission companions are capped at 4 MiB, 4,096 externs, 256 parameters
and 512 bytes per line. File-writing binding commands require POSIX;
snapshot publication currently requires Linux.

CTest registers always-available `on1x.contracts` semantics/source tests and
optional `on1x.contracts_sdk` transport/ABI/discovery tests. Counted allocator
failure loops check partial JSON trees, serialization and Parsec captures; Vec
tests check failed growth, internal aliases, logical removal and resource-free
byte copying. SDK tests deliberately assign review flags to **test fixtures**
only to test gates; they do not review the real annotations. See
[verification](../VERIFICATION.md) for executed configurations and the current
Clang/libstdc++ dependency-build limitation.
