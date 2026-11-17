# MIR relational analysis v1

The opt-in B3 scalar analysis layer uses the real `Novostone::Datalog` and
`Novostone::CCDB` libraries. It projects **verified On1x MIR metadata** into typed
relations and computes CFG reachability, transitive calls, conservative effects,
and block liveness. All executable semantics remain in [MIR](backend-mir.md).

This mapping accepts **MIR v1 only**. Numeric/product MIR v2 has new type and
opcode descriptors not yet represented by the relational schema; both source
and saved-v2 analysis reject before publishing facts. The v2 execution paths
remain usable without Novostone. No float bit payload is narrowed or treated
as integer arithmetic to force admission to this mapping.

## Build and use

From the MuLIB root:

```sh
cmake -S on1x -B /tmp/opencode/on1x-analysis \
  -DON1X_REQUIRE_LIBSTONE=ON -DON1X_WITH_NOVOSTONE=ON \
  -DON1X_LIBSTONE_ROOT="$PWD/on1x/third_party/libstone" -DBUILD_TESTING=ON
cmake --build /tmp/opencode/on1x-analysis --parallel 4
ctest --test-dir /tmp/opencode/on1x-analysis --output-on-failure
/tmp/opencode/on1x-analysis/on1xc --emit-ir-facts on1x/examples/analysis.on1x \
  -o /tmp/opencode/analysis.dl
/tmp/opencode/on1x-analysis/on1xc --analyze-ir on1x/examples/analysis.on1x \
  -o /tmp/opencode/analysis.sexpr
/tmp/opencode/on1x-analysis/on1xc --emit-ir on1x/examples/analysis.on1x \
  -o /tmp/opencode/analysis.mir
/tmp/opencode/on1x-analysis/on1xc --ir-input --analyze-ir /tmp/opencode/analysis.mir
```

The example has mutually recursive `even`/`odd` functions and a loop in `main`.
Execution prints `true`, `false`, `true`, `false`. Its call closure includes
`even -> even` and `odd -> odd` because they participate in a recursive cycle.

`--emit-ir-facts` emits ordinary Novostone `.dl` source containing the complete
catalog and base facts. It can be queried by the upstream CLI, e.g.
`novostone datalog /tmp/opencode/analysis.dl Calls`. `--analyze-ir` emits a
deterministic S-expression containing mapping version, CCDB revision, **the exact
canonical MIR text**, canonical schema/rule identity, and query names, base
dependencies and rows. Results are sorted by Novostone tuple order. Source paths
are identity-bearing; moving source files changes artifact bytes. Cache-hit
state and elapsed time do not enter this report.

Both flags accept the usual source/import/macro/PPX path or `--ir-input` saved
MIR. Builds without `ON1X_WITH_NOVOSTONE` reject them explicitly (exit 2).
The option defaults OFF and is independent of ExoML and scanner selection.

### Installed SDK

Instead of the supplied source checkout, select an installed package explicitly:

```sh
cmake -S on1x -B /tmp/opencode/on1x-analysis-package \
  -DON1X_USE_FALLBACK_SCANNER=ON -DON1X_WITH_NOVOSTONE=ON \
  -DON1X_NOVOSTONE_PROVIDER=PACKAGE -DCMAKE_PREFIX_PATH=/path/to/novostone-prefix
```

`SOURCE` (default) uses the checkout's CMake targets and build-time Perl
generators. `PACKAGE` uses `find_package(Novostone CONFIG REQUIRED)` and the
installed public headers/libraries. A missing requested provider fails configure.
Only Datalog and CCDB are linked into the compiler; generated applications still
link their C11 runtime. `on1x-build-info.txt` records selection, source checkout
identity when available, package directory/version, and the On1x rule-file hash.
An installed package's upstream commit is not inferred from an unrelated source
checkout. See [measured verification](../VERIFICATION.md).

## Mapping and query semantics

`src/backend/Analysis.dl` is the single authoritative schema/rule catalog. CMake
embeds it, and Novostone's Aurocks/ASDL parser validates it once; there is no
duplicate rule parser or runtime source-file lookup.

All identities and source coordinates use **canonical unsigned decimal symbols**.
This preserves MIR's entire `uint64_t` ID domain, including values above
`INT64_MAX`. Ordinals and literal constants use signed 64-bit integer columns.
Constants retain `INT64_MIN` and `INT64_MAX`; they are never converted to
Novostone's separate wrapping-i32 execution dialect. Symbol sorting is lexical,
not numeric; clients should compare identity strings or decode them as `uint64_t`.

Base relations (column order is part of mapping v1):

| Relation | Columns |
|---|---|
| `Module` | module ID, MIR version, mapping version |
| `Function` | function ID, ordinal, name, result type |
| `Entry` | function ID, entry block ID |
| `Block` | block ID, function ID, ordinal |
| `Slot` | slot ID, function ID, ordinal, type |
| `Parameter` | value ID, block ID, ordinal, type |
| `Instruction` | instruction ID, block ID, ordinal, opcode, type |
| `Constant` | instruction ID, exact signed immediate |
| `Operand` | instruction ID, ordinal, value ID |
| `Call` | instruction ID, caller ID, callee ID |
| `SlotAccess` | instruction ID, slot ID |
| `Terminator` | terminator ID, block ID, kind |
| `ExitValue` | terminator ID, returned value or branch condition ID |
| `Edge` | terminator ID, edge ordinal, source block ID, destination block ID |
| `Argument` | terminator ID, edge ordinal, argument ordinal, value ID |
| `BlockEffect` | block ID, effect parameter ID |
| `EffectStep` | instruction ID, input token, output token, effect kind |
| `ExitEffect` | terminator ID, final effect token |
| `EffectEdge` | terminator ID, edge ordinal, final token, destination token |
| `Location` | entity ID, file, byte offset, line, byte column |

Effect-definition locations are inherited from the defining block/instruction,
as in MIR. Edge argument positions correspond to destination parameter positions;
two branch edges to the same destination retain distinct ordinals and arguments.

Derived relations:

| Query | Meaning |
|---|---|
| `Reachable(function, block)` | Reachable from that function's entry, including entry |
| `Calls(caller, callee)` | One or more direct call edges; self only when cyclic |
| `MayEffect(function, kind)` | Local and transitive `trap`, `memory`, `call`, `io` effects |
| `Definition(block, value)` | Block parameters and non-void instructions |
| `UseAt(block, value)` | Ordinary instruction operands and terminator values |
| `LiveIn(block, value)` | Values needed before the block's parallel parameters |
| `LiveOut(block, value)` | Successor live-ins and outgoing edge-argument uses |

All CFG branches are considered possible, even constant-false branches. Liveness
is conservative: every edge argument is considered used even if its destination
parameter is dead. Phi arguments are uses on predecessor edges, not ordinary
uses inside their destination. Slots and effect tokens are separate from scalar
SSA liveness. `MayEffect` records calls as barriers and does not prove termination,
purity, alias freedom, ownership, or legality of deleting/reordering instructions.

## C++ API and revision contract

Link `on1x_novostone` and include `on1x/backend/relational.hpp`:

```cpp
namespace r = on1x::mir::relational;
r::Store store(verified_mir);
auto source = store.snapshot();
auto calls = source.query("Calls");
source.require_current(calls);
auto next = store.replace_function(source, replacement_function);
// next.require_current(calls) now rejects: calls belongs to source.
auto current_calls = next.query("Calls");
```

`Store` verifies the complete candidate MIR and builds its full projection before
publishing an atomic CCDB transaction. `replace_function` uses the stable function
ID, then verifies all callers and the resulting module. Only fact insertions and
deletions are staged. Commit validates exact projected facts. Malformed MIR,
budget failure, stale or foreign snapshots leave the previous head intact.

CCDB caches track transitive base-relation dependencies. Unrelated location or
constant changes can retain call results; changing/deleting `Call` facts invalidates
the call closure. This is relation-level invalidation with recomputation, not
tuple-level differential maintenance. Projection itself currently visits the
whole module, including on a single-function replacement.

A `Query` owns its immutable source snapshot and exact MIR identity. Old snapshots
remain usable after subsequent publications or store destruction. To consume an
analysis for a candidate revision, call `require_current`: equality of numeric
revision counters alone is insufficient, including across two different stores.
Queries cannot be constructed from arbitrary rows. Store mutation is single-threaded;
the underlying immutable CCDB snapshots support independent readers.

`Store::optimize(expected, options, import_limits)` runs the
[verified scalar pass pipeline](backend-passes.md) and publishes its verified
result against the exact source snapshot. Changed publications invalidate queries
through actual fact deltas. No-op results preserve the current revision; failed
projection leaves it intact. The returned pass result records complete versus
budget-limited progress and detailed transformation provenance.

`project` exports a typed Datalog module. `check_projection(mir, parsed_facts)`
requires the exact current rule catalog and verifies all base facts against the
given MIR; missing, extra, changed, mistyped, unknown or derived facts reject.
Omitted empty base relations are normalized. The CLI does not ingest fact files
as executable MIR or trusted cached analyses.

## Limits and verification boundary

- MIR verification retains its own node/work limits.
- Projection defaults: 262,144 facts and 8 MiB of fact payload (relation names,
  symbol bytes and eight bytes per integer; not total allocator usage).
- Each API query defaults to Novostone's 1,000,000-fact / 10,000,000-work limits.
  Non-default limits remain enforced after a prior query has been cached.
- Reports share one cumulative work budget across their five queries. CLI
  `--analysis-work N` overrides it, including zero to force exhaustion. Exhaustion
  returns 75 and never emits a partial result or overwrites an existing output.
- Reports use the existing bounded S-expression writer (4 MiB / 262,144 nodes),
  with node accounting before building row trees. Fact dumps are bounded by the
  projection; callers parsing large `.dl` exports must select suitable upstream
  `ParseLimits` (upstream defaults are 1 MiB / 10,000 statements).

`on1x.relational_hir` compares seven queries against independent MIR graph/set
algorithms and Novostone's naive evaluator, including 40 seeded multi-function
CFGs, recursive calls, loops, independently authored cyclic phis and parallel
edges. It tests catalog/fact tampering, exact wide constants/IDs, source identity,
snapshot lifetime, stale/foreign results, cache preservation/invalidation,
deletion retraction, failed publication and independent budgets.
`on1x.relational` tests the CLI and deterministic source/saved-MIR artifacts,
located rejection, exhaustion, output preservation, and disabled-feature errors.

This is a scalar B3 increment with B4 built-in-pass publication. General
resource/contract facts, external analysis-driven transforms, Sequent integration,
and VM/native lowering remain future backend work. The built-in transforms use
verified MIR directly; fact rows are not rewrite authority.
