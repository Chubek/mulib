# mulib-lir

[Manuals](README.md) · [Library index](index.md)

Owned, terse 64-bit register LIR with extensible per-record annotations. The
semantic core reuses `mulib_mir_native_instruction_t`: constants, parameters,
copies, integer operations, memory, direct calls and explicit block terminators.
This preserves native MIR's wrapping arithmetic, masked shifts, initially-zero
registers, and fixed-arity ABI. Checked SSA and runtime-flow MIR must first be
legally lowered to this native dialect; silently changing their overflow or
suspension semantics is not supported.

## Integration

Define these switches in one C11 translation unit before any includes:

```c
#define MULIB_IMPL_MIR
#define MULIB_IMPL_MUBURG
#define MULIB_IMPL_MACHINE
#define MULIB_IMPL_LOWOPT
#define MULIB_IMPL_LIR
#include "mulib-lir.h"
```

`mulib_lir_from_mir` validates and deep-copies a native program, including symbols
and call arguments. It initializes identity order and source-record IDs.
`mulib_lir_clone` also copies all annotations and selection state. Both require
a zero output and preserve it on failure. `mulib_lir_free` releases all storage
and zeroes the owner. Never shallow-copy a live owner. The target descriptor alone
is borrowed and must remain immutable/alive; built-in targets are static.

The limit bounds each MIR array and the target rule count. Allocation sizes are
checked before multiplication. All statuses use `mulib_mir_status_t`.

## Annotation contract

Each record has:

* `source`: original MIR record ID, retained through rewrites and NOP removal.
* `constraints`: caller-supplied `KEEP`, `BARRIER`, and/or `VOLATILE` bits.
  KEEP prevents rewriting/removal; BARRIER and VOLATILE additionally prohibit
  scheduling across the record. Constrained records are retained by OptML.
* `known_mask`, `values`: proven constant operands, refreshed by selection.
* `latency`, `throughput`, `cycle`: positive duration, resource occupancy, and
  block-relative issue time. These are model estimates for a lowered bundle.
* `resources`: shared unit-capacity resource bitmask, refreshed from ArchML.
* `schedule[64]`: optional NUL-terminated ArchML schedule-class override; empty
  means infer the class from the current opcode. An unknown override is an error
  when an ArchML model is supplied. Names use ASCII letters, digits, `_`, `-`, `.`.
* `barrier`: derived ArchML barrier fact, refreshed with the tuning model.

Selection records retain the chosen target rule, instruction ID, literal operand
mask, literals and cost. `order[position]` maps issue order to semantic record ID;
block IDs, argument slices and record IDs remain stable. `block_cycles` gives
the completion makespan per block. Removed records become NOPs.

After editing the semantic program or constraints, set `selected = scheduled = 0`
and call `mulib_isel_refresh`. Do not forge selection or constant facts. The
refresh API is transactional and ignores stale derived selection facts while
preserving constraints, schedule overrides and provenance.

`mulib_lir_validate` checks semantic records, target, annotation shape, selected
rules, operand constants and costs against independently recomputed block-local
facts. Scheduled LIR additionally checks order permutations, register/memory/barrier
dependencies, issue cycles, resource occupancy and block makespans. Unselected or
unscheduled LIR may carry stale derived facts or order, respectively; diagnostic
listings use semantic order when unscheduled. Validation does not prove custom
rewrites equivalent or reconstruct a released ArchML model. Emission requires
both selection and scheduling to be current.

`mulib_lir_reads` includes CALL arguments. `mulib_lir_dependency` identifies
RAW/WAR/WAW, ordered memory, call, barrier and terminator dependencies in validated
blocks; these helpers also support custom passes.

## Listing and example

`mulib_lir_write` emits a deterministic diagnostic text listing with `@source`,
`@constraints`, `@known`, `@cost`, `@latency`, `@throughput`, `@cycle` and
`@schedule`. Numeric operand fields use native MIR's `SIZE_MAX` sentinel. This is
a diagnostic serialization; construct executable LIR through the C API.
Writers return nonzero on success; a failed writer may have partial output.

```sh
cc -std=c11 -Iinclude examples/modules/lir.c -lm -o lir
./lir
```

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-lir.h`](../include/mulib-lir.h) |
| Implementation switch | `MULIB_IMPL_LIR` |
| Repository-local includes | [mulib-lowopt](mulib-lowopt.md), [mulib-machine](mulib-machine.md) |
| Complete example | [`examples/modules/lir.c`](../examples/modules/lir.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/lir.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
