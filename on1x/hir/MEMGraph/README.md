# MEMGraph/v1 — typed memory-graph rewrite descriptions for On1x

The separate [resource speculation and private scratch profile](RESOURCES.md)
runs on checked HIR and verified MIR independently of authored bounds rules.

**Status:** `on1xc` uses the compiled `c11_fixed_array_v1` profile to omit
provably redundant fixed-array index and slice checks in direct C11 and
VM-product MIR. The existing rules and profile identifier are unchanged.
It covers literal and small closed checked-`i64` bounds expressions; dynamic
or potentially trapping bounds retain the reference checks.
The independent C++ [lexical slot plan](ADAPTER.md#checked-lexical-slots)
projects certain scalar loan facts from canonical verification, without
executing authored ownership rules. Those rules and the broader type,
dynamic slice provenance and FFI rules remain design descriptions, not
compiler plugins or safe-FFI admission. No new syntax or runtime ABI is
introduced.

This package is derived from the idiom and real grammar syntax of
[LibStone cornerstone/LLSynth](https://github.com/Chubek/libstone/tree/master/cornerstone/LLSynth)
and the existing On1xMETA language. `MEMGraph.llsynth` is a **real LLSynth
language specification**, not Python or an On1x grammar. CMake runs LibStone's
LLSynth and compiles its parse trees into immutable tables for the compiler.
The `.mmg` files declare passes; `.mmgi` includes contain patterns.
Emitted programs do not need LLSynth, Perl or Python.

## Local rewrites in lowered MIR

MIR lowering retains the legacy `local-memory` hint for straight-line i64
scratch use. Separately, it requests the scoped `memgraph-local` bank when a
typed slot load has an earlier access to that slot in the same block, or
two stores in that block write the same SSA value to the same slot. The bank
currently selects `mir-c11` with the exact target `x86_64-unknown-linux-gnu`:
unsigned and IEEE scalar loads request safe rules; tuple loads and repeated
stores request speculative rules. A function can request both.
The specializer must still prove the reaching value, and speculative
requests need host opt-in. These requests neither assert ownership or
placement nor depend on the optional bounds profile.

## Layout

```
on1x/hir/MEMGraph/
  MEMGraph.llsynth             LLSynth lexical/grammar specification
  00-types.mmg                 post-inference invariant checks
  10-memory.mmg                capacity, indexing, provenance checks
  20-ownership.mmg             move/drop/borrow checks
  30-ffi.mmg                   admission and raw-FFI separation
  40-canonical.mmg             guarded candidate optimizations
  50-c11-fixed-array.mmg       executable fixed-array profile
  types.mmgi, memory.mmgi, c11-fixed-array.mmgi, owners.mmgi, loans.mmgi, ffi.mmgi
  canonical-types.mmgi, canonical-memory.mmgi,
  canonical-owners.mmgi, canonical-ffi.mmgi
  MEMGraph.manifest.json       generated, NOT an admission certificate
  tools/mmgc.py                deterministic contract/table compiler
  tools/preview.py            candidate-only (unverified) graph matcher
  tests/test_mmgc.py            parser, determinism and rejection tests
  tests/test_integration.py     strict C11 and VM differential tests
  tests/test_lexical_slots.cpp  checked scalar lexical plan and negative tests
  tests/fixed-array.on1x        executable array fixture
  tests/demo_graph.json        abstract sample input
  ADAPTER.md                   implemented profile and future proof contract
```

## Build and check

From the MuLIB repository root:

```sh
cmake -S on1x -B build/on1x -DBUILD_TESTING=ON -DON1X_WITH_MEMGRAPH=ON
cmake --build build/on1x --target on1xc on1x_memgraph_hir_tests on1x_memgraph_lexical_slots_tests
ctest --test-dir build/on1x -R '^on1x\.memgraph\.' --output-on-failure
build/on1x/on1xc --dump-memgraph on1x/hir/MEMGraph/tests/fixed-array.on1x
build/on1x/on1xc --emit-c --enable-opt memgraph-bounds program.on1x -o program.c
build/on1x/on1xc --run-vm --enable-opt memgraph-bounds program.on1x
```

The optimization is off in the normal and debug profiles and on in the release
profile for direct C11 and source-to-VM. Use `--disable-opt memgraph-bounds` to produce the
reference output, or to disable the analysis in `--dump-memgraph`. A named
`memgraph-bounds` selector is not applicable to standard MIR, MIR-native
emission, or serialized `--ir-input`.
With `ON1X_WITH_MEMGRAPH=OFF`, direct C11 emits its reference checks and
`--dump-memgraph` reports that bounds support is unavailable. The
`on1x.memgraph.lexical_slots` test remains available in that configuration.

The real driver is under
`ON1X_LIBSTONE_ROOT/cornerstone/LLSynth/scripts/llsynth.pl` in the bundled
checkout (CMake also recognizes `ON1X_LIBSTONE_ROOT/LLSynth`). CMake enables
MEMGraph by default if LLSynth and Perl are present; explicitly enabling it
without them fails configuration. The target `on1x_memgraph_tables` builds
just the generated parser, immutable table header and build manifest.

On [`tests/fixed-array.on1x`](tests/fixed-array.on1x), MEMGraph removes 11 of
15 generated bounds calls: eight sites with literal bounds and three with
computed bounds. The four dynamic or slice-backed calls stay in place. This
is a code-generation result, not a general speedup claim.

To generate a manifest with the real parser independently of CMake:

```sh
python3 on1x/hir/MEMGraph/tools/mmgc.py --root on1x/hir/MEMGraph \
  --llsynth on1x/third_party/libstone/cornerstone/LLSynth/scripts/llsynth.pl \
  --manifest /tmp/memgraph-plan.json
```

To preview the structure of candidate rewrites on an **abstract** JSON graph
without evaluating proofs or changing the input:

```sh
python3 on1x/hir/MEMGraph/tools/preview.py \
  on1x/hir/MEMGraph/tests/demo_graph.json
```

The preview output is prominently marked `UNVERIFIED_DO_NOT_PUBLISH`. It is
useful for reviewing pattern matching, never for changing real On1x HIR.

`--llsynth` invokes the real `llsynth.pl --mode=interpret -T python3` and
parses **every** `.mmg` and `.mmgi`. `mmgc.py` validates the resulting rule
contracts. Without `--llsynth` or `--parser`, the standalone checker can
validate and preview the design package, but cannot emit compiler tables.

To check only grammar generation, LibStone also supports:

```sh
perl on1x/third_party/libstone/cornerstone/LLSynth/scripts/llsynth.pl \
  --check --mode=interpret on1x/hir/MEMGraph/MEMGraph.llsynth
```

## DSL

`.mmg` describes a single named pass:

```text
memgraph 1;
budget steps 1048576 nodes 131072 depth 128;
pass ownership stage post_ownership mode verify priority 700;
include "owners.mmgi";
include "loans.mmgi";
endpass;
```

The optional `profile c11_fixed_array_v1;` directive (after the pass
declaration, before includes) selects the only executable profile. The build
restricts it to `pre_codegen` rewrite mode and checks its shapes and exact
obligation lists. Other passes are validated but do not generate executable
tables.

`.mmgi` groups rules for one pass:

```text
memgraph 1;
budget steps 1048576 nodes 131072 depth 128;
patterns canonical;
rewrite scalar_copy_collapse priority 250
match CopyScalar(CopyScalar($value))
replace CopyScalar($value)
requires typed_hir, copy_safe_scalar, copy_no_trap, no_hidden_effect
preserves value, type, evaluation, effects, ownership, loans, ffi, traps, provenance;
endpatterns;
```

Three rule kinds are supported:

* `check NAME priority N match TREE requires OBLIGATIONS diagnostic "MMG-...";`
  — reject a matching graph node unless **all** conditions are proven by the
  host; lack of information is a failure.
* `reject NAME priority N match TREE when OBSERVABLE_BAD_FACTS diagnostic "MMG-...";`
  — reject if **all** listed negative facts are independently established.
  Unknown facts alone do not satisfy a `reject` rule; the companion `check`
  rules establish fail-closed obligations for admitted operations.
* `rewrite NAME priority N match TREE replace TREE requires OBLIGATIONS
  preserves VALUE_LIST;` — propose a typed graph replacement; **never** grant
  proof or publish it without independent checking.

`TREE` is a constructor invocation with fixed arity, or `$capture` (only under a
constructor). Node spellings are **MEMGraph normalized-IR operators**, not
`Expr::Kind` spellings or C calls. The manifest validator rejects unknown
operators and wrong arities. String leaves are allowed for future extensions;
current rules deliberately use only graph captures.

## Safety invariant

A `.mmgi` file is untrusted input. Rule authors cannot mint type evidence,
move/borrow authority, FFI approval, target ABI identities, or purity claims.
A guard is **a requested independently evaluated proof obligation**, not an
assertion accepted at face value. `preserves` lists what the host MUST check;
it does not certify preservation.

All candidate rewrites use exactly the same captures, exactly once and in
left-to-right evaluation order. All nine preservation domains are mandatory.
The host must also check the structural graph, source HIR, entire proposed
candidate, and all relevant original and candidate semantic invariants.
An unsupported guard or exceeded work budget rejects the candidate; the
original checked HIR remains untouched. The engine must never rewrite raw
source text by regex, infer safety from `MEMGraph.manifest.json`, or treat an
ExoML header claim as signed independent review.

### Rewriting limitations

- Rewriters must preserve error/trap ordering; **no** elimination of unchecked
  arithmetic, bounds checks that may trap, C-string NUL checks, error handling, `drop`,
  `retain`, foreign calls, or user-visible allocation-failure behavior by default.
- `BoundsValidated` and `TypeValidated` are abstract *proof-associated* nodes;
  collapsing nested nodes requires an independent proof of identical
  validation scope and no intervening state change, not a textual match.
- `ReadOnlyView` coalescing requires equal lexical scopes and loan roots.
  A nested `borrow` in real HIR cannot simply be deleted.
- `AbiCheckedScalar` coalescing is limited to independently admitted, exactly
  typed, total ABI checks; arbitrary pointer or borrowed-string transforms are
  explicitly absent.
- Safety checks on `ForeignCall` must use ExoML/Chisel/review evidence provided
  and validated by the real host. `UnsafeForeignCall` remains explicitly unsafe.
- No `Send`/`Sync`, general foreign-owner ABI, async suspension, or MIR
  ownership support is implied by this package.

## Determinism and limits

Files are UTF-8, at most 256 KiB each; includes must be simple, existing,
non-symlink sibling `.mmgi` files. No nested includes, filesystem traversal,
or remote fetches. The package allows at most 256 rule/pass files and 4,096
rules; each pass allows at most 512 rules. Rules are keyed by name and sorted
by descending priority;
passes are sorted by stage, descending priority, then name. Names are unique
within a pass; unknown conditions, repeated/dropped/reordered captures,
unknown graph nodes, stale or orphan includes, inconsistent budgets, wrong
file category, and unsupported revision cause rejection. The JSON manifest
contains source SHA-256 fingerprints and declares that host admission is
required. The compiler links only the enabled profile's validated rule tables,
not the manifest or an arbitrary user-supplied package.

## Current status / what remains

Only the constant fixed-array bounds rules are executable. The host lexical
slot projection is narrow; dynamic slice provenance, general ownership and
loan traces, FFI review, and general MIR memory/effect descriptors still
need separate executable profiles. The design-only rules
request explicit checks for slice lifetime and range order, current owner
generation, generic type constraints, and foreign pointer ownership. See
[ADAPTER.md](ADAPTER.md) for the evidence those profiles would require.
