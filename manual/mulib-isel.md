# mulib-isel

Memoized minimum-cost tree covering and native MIR → annotated LIR selection.

## Integration

Define all transitive switches before including `mulib-isel.h`:

```
MULIB_IMPL_MIR MULIB_IMPL_MUBURG MULIB_IMPL_MACHINE MULIB_IMPL_LIR
MULIB_IMPL_ISCHED MULIB_IMPL_PARSEC MULIB_IMPL_ARCHML MULIB_IMPL_LOWOPT
MULIB_IMPL_ISEL
```

Implementations are C11; declarations have C linkage. Link `-lm` for MIR's runtime
implementation. No dependency switches are implicitly enabled.

## Memoized tree covering

`mulib_isel_cover` accepts MuBURG's node/rule structures, a root and a requested
nonterminal goal. A rule has one opcode root, up to two child goals, a nonnegative
cost and a target instruction ID. Matching recursively requests `(node, goal)`
states, caches each result and detects active-state cycles. Rules are compared
by additive cost, with stable rule-order ties. The returned sparse cache contains
only demanded states, including unsuccessful alternatives. This is a top-down
memoized recurrence, rather than the existing MuBURG eager bottom-up table fill.

The API supports arbitrary expression trees and DAGs; repeated DAG children are
costed as repeated tree occurrences, not a global shared-DAG optimum. There are
no epsilon or chain rules. The plan retains node/rule indices, not pointers into
the input arrays. `memo[root]` identifies the selected root rule. Following each
rule's child goals through memo entries reconstructs the complete cover.

Structural validation covers every input node/rule. Cycle detection covers
demanded states. Depth is explicitly bounded and capped at 128 to protect the C
stack. Node, rule, goal and cache-cell counts, input validation and memo/rule
lookup work have literal limits. The cache grows only as states are demanded,
even when the theoretical node/goal product overflows. Empty roots are invalid. Overflowing alternatives are ignored
when a finite cover exists; no finite cover returns UNSUPPORTED or OVERFLOW.
Failure preserves an empty output; release with `mulib_isel_cover_free`.

## Native MIR lowering

`mulib_isel_run(program, options, &lir, diagnostic)` deep-copies and validates a
native MIR program, selects each operation, computes semantic annotations and
schedules the resulting LIR. Built-in machine grammars cover one operation and
its register/immediate leaves. Shared values are kept in virtual-register slots;
effectful or multiply-used definitions are not duplicated by tree expansion.
Known constants propagate within blocks, including copies and wrapping arithmetic.
Immediate ranges come from the selected machine grammar.

Defaults select ELF AMD64 SysV. Set `options.machine = mulib_machine_rv64im()` for
ELF RV64IM LP64. These reuse the existing ABI-aware spill-slot assembly backends.
Custom `mulib_machine_t` descriptors supply their own grammar and emitter. Other
ArchML architecture descriptions alone do not supply an assembly backend.

`mulib_isel_refresh` transactionally refreshes edited LIR. It preserves source
IDs, caller constraints and explicit schedule-class overrides. The options target
must match the LIR's target. `mulib_isel_schedule` refreshes just scheduling on
already selected LIR. On failure the original owner is unchanged.

## ArchML scheduling

Load `library/archml/amd64.aml` or `riscv64.aml` with `mulib_archml_load` and assign
`options.arch`. The target/profile names must match the built-in backend. The
document is borrowed only during the call; no LIR pointer retains its storage.
Without a document, the existing target's rule latencies are used.

Default schedule classes are `alu`, `mul`, `load`, `store`, `serial` (CALL), and
`branch` (terminators). Explicit `annotations[k].schedule` overrides are allowed.
The adapter reads ArchML `latency`, `throughput-cycles`, `uses` resource references
and `barrier`, rejecting missing or invalid model entries. Up to 64 resources are
supported. Resource capacities are conservatively treated as one even when the
profile allows more. The current model is single-issue and bundle-level: ABI
loads, spills and instruction expansions are not independently scheduled.

Each block gets RAW, WAR and WAW edges, all memory-effect pairs, CALL argument
dependencies, full call/barrier ordering and terminator edges. The resource-aware
`mulib_isched_run_resources` chooses critical-path order and delays issue for
latency/resource occupancy. `schedule=0` preserves input order while still
computing valid cycle/resource annotations.

Defaults bound each structural array to 65536 entries, total edges to 262144,
block makespan to 1048576 cycles and selection/scheduling to 16777216 work units.
One budget covers all local covers, register resets, dependency discovery and
resource matching in a selection/refresh call. Scheduling reserves a conservative
O(nodes² + nodes*edges) scan budget before running each block; an independent
scheduling call gets its own budget. Limits are literal, including zero.
No execution delays or NOP padding are emitted merely
to match the annotated cycle model.

## Example and checks

```sh
cc -std=c11 -Iinclude examples/modules/isel.c -lm -o isel
./isel
cc -std=c11 -Iinclude examples/modules/optml.c -lm -o optml
./optml library/archml/amd64.aml > twice.s
```

`tests/test_isel.c` covers sparse/shared-state costs, alternatives, overflow,
cycles, resource stalls, real ArchML profiles, randomized native semantics and
loop/control flow. `tests/test_isel_alloc.c` fails every pipeline/refresh
allocation and checks rollback and leaks. `tools/check_isel_native.py` assembles
and executes AMD64 output and cross-assembles RV64 with Clang when available.
