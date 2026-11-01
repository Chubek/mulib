# mulib-microcc

MicroCC compiles one function from five high-level IRs (or native MIR) into
standalone assembly. Built-in targets are **ELF x86-64 System V** and **RV64IM
LP64**. Compilation is portable C11; it does not need to run on the target.

## Integration

Define these switches in exactly one translation unit, before including
`mulib-microcc.h`:

```c
#define MULIB_IMPL_MIR
#define MULIB_IMPL_GSSAIR
#define MULIB_IMPL_CLICKIR
#define MULIB_IMPL_ANFIR
#define MULIB_IMPL_SUPERBLOCKS
#define MULIB_IMPL_DEPIR
#define MULIB_IMPL_MUBURG
#define MULIB_IMPL_MACHINE
#define MULIB_IMPL_ISCHED
#define MULIB_IMPL_LOWOPT
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_MICROCC
#include "mulib-microcc.h"
```

Link `-lm` for MIR's existing floating runtime on platforms requiring it.
Declarations have C linkage for C++ clients. No dependency implementation switch
is enabled implicitly. The generated assembly needs **no MuLIB runtime**.

## Pipeline and design decisions

1. [ClickIR](mulib-clickir.md) orders block-pinned Sea of Nodes graphs;
   [ANFIR](mulib-anfir.md) materializes literal atoms;
   [GSSA](mulib-gssair.md) checks types, dominance and memory chains;
   [DepIR](mulib-depir.md) derives execution graphs from entity dependencies;
   [Superblocks](mulib-superblocks.md) supplies single-entry trace layout.
2. All lower into `mulib_mir_native_program_t`: mutable virtual registers,
   explicit basic blocks, loads/stores and direct calls. PHIs become **parallel
   copies on split edges**. Temporary registers handle copy cycles; critical
   edges get their own blocks. Existing checked SSA/VM MIR semantics are separate.
3. [LowOpt](mulib-lowopt.md) folds/simplifies native MIR **before** selection,
   so no stale instruction covers or dependencies survive a rewrite.
4. [Machine](mulib-machine.md) and MuBURG select a minimum-cost instruction
   cover per operation and its operand leaves. Shared values are materialized
   once, not recursively duplicated as trees. The optimum is for the supplied
   local grammar, not for a whole DAG, register allocation or the final binary.
5. ISched schedules within each block using the selected rule's latency. Edges
   preserve register RAW/WAR/WAW hazards, all memory/call ordering, and the final
   terminator. It neither speculates memory accesses nor moves work across
   side exits. Issue cycles are a single-issue model; no delay NOPs are emitted.
6. The target handles stack locations, calling convention, labels and assembly.
   Built-ins assign every virtual register a stable 8-byte spill slot and use
   reserved scratch registers. This works for arbitrary liveness, loops, calls
   and register pressure. Frame offsets are checked before emitting.

This fills gaps between selection, scheduling and actual native execution:
memory ordering, PHI destruction, ABI state and register placement are explicit.

## API and ownership

```c
mulib_microcc_source_t source = { MULIB_MICROCC_GSSAIR, &ssa_program };
mulib_microcc_options_t options = mulib_microcc_defaults();
mulib_microcc_result_t result = {0};
mulib_microcc_diagnostic_t diagnostic;
/* options.machine = mulib_machine_rv64im(); */
mulib_mir_status_t status = mulib_microcc_compile(
    &source, "my_function", &options, &result, &diagnostic);
/* On success, consume result.assembly/result.assembly_length. */
mulib_microcc_free(&result);
```

`source.program` must point to the descriptor matching its tag:

| Tag | Descriptor |
|---|---|
| `MULIB_MICROCC_NATIVE` | `mulib_mir_native_program_t` |
| `MULIB_MICROCC_CLICKIR` | `mulib_clickir_program_t` |
| `MULIB_MICROCC_ANFIR` | `mulib_anfir_program_t` |
| `MULIB_MICROCC_GSSAIR` | `mulib_gssair_program_t` |
| `MULIB_MICROCC_DEPIR` | `mulib_depir_program_t` |
| `MULIB_MICROCC_SUPERBLOCKS` | `mulib_superblocks_program_t` |

The source and target are borrowed only during compilation. Successful output
owns native MIR arrays and copied symbol names, selections, order, per-record
issue cycles, per-block makespans and NUL-terminated assembly. The caller may
release the source immediately afterwards. Never copy or edit a live result;
free is NULL-safe and resets it. A destination must be zero-initialized/empty.
Compilation failure leaves it unchanged. There is no global compiler state.

`result.order[position]` indexes the original, lowered/optimized MIR record;
`starts[record]` is block-relative, and `block_cycles[block]` is its makespan.
`cost` is the sum of local grammar costs. `optimization` reports fold,
simplification and removal counts. NOPs retain stable record IDs.

`mulib_microcc_write` invokes a writer once with the assembly bytes. Return
nonzero from the writer for success. A failing writer can have partial external
effects but does not alter the artifact. Compilation itself buffers emission
transactionally, including custom-target errors.

## Bounds and diagnostics

`options == NULL` uses defaults. Zero limits are actual bounds:

| Option | Default / meaning |
|---|---|
| `machine` | `mulib_machine_x86_64_sysv()` |
| `limit` | 65536; each IR/selection array, rule count and SSA block-squared analysis cells |
| `edge_limit` | 262144; aggregate scheduling edges across all blocks |
| `cycle_limit` | 1048576; makespan per block |
| `text_limit` | 1048576; assembly bytes excluding NUL |
| `flags` | `MULIB_MICROCC_OPTIMIZE | MULIB_MICROCC_SCHEDULE` |

Clear either flag to disable that pass. Without scheduling, the order is
unchanged and issue times conservatively serialize operation latencies.
Array-size, frame-offset, aggregate cost and scheduling arithmetic overflow are
rejected. Allocation failure returns `NO_MEMORY`; missing covers return
`UNSUPPORTED`. Statuses use `mulib_mir_status_t`. Diagnostics report a pipeline
stage and that stage's record index, not an original source location. A lowered
or reordered record can have a different ID from its high-level source node.
The target grammar and source arrays must be physically readable C objects.
Scheduling examines instruction pairs; SSA validation uses dense block analysis.
These limits bound storage, not elapsed compilation time.

## Native profile and assembly tools

Words and integer operations are 64-bit; addition/subtraction/multiplication
wrap, shifts mask their count to 0..63, and comparison results are 0/1. There
are no checked-overflow operations, division, floating-point operations, varargs,
aggregate ABI values, indirect calls, atomics, exceptions or unwind metadata in
this dialect. Unknown opcodes/types fail validation. Memory operands are native
addresses supplied by the caller, and LOAD/STORE access one full native word;
the caller supplies valid, aligned storage. External functions must implement
their declared word-valued signatures. See the MIR and target manuals.

Symbols are ASCII identifiers, 1..127 bytes. Emit each function under a distinct
symbol; generated local labels incorporate it. Assembly can be concatenated.
The output is GNU/LLVM assembler syntax. MuLIB's existing assembler consumes
typed instruction operands and lacks memory/text parsing, so this pipeline uses
an external assembler/linker rather than pretending its output fits that API.

```sh
cc -std=c11 -Iinclude examples/modules/microcc.c -lm -o microcc
./microcc > add42.s
cc -c add42.s -o add42.o
# Link with a C caller declaring uint64_t mulib_generated_add42(uint64_t).
./microcc rv64 > add42-rv64.s
clang --target=riscv64 -march=rv64im -mabi=lp64 -c add42-rv64.s
```

## Verification

`tests/test_microcc.c` covers all input forms, literals, cyclic PHI copies,
critical edges, malformed graphs, bounds, local cover choice and target changes.
`tests/test_microcc_alloc.c` fails each allocation through all six pipeline
inputs and structural conversions, checking atomicity and leaks.
`tools/check_microcc_native.py` assembles, links and executes x86-64 output against
independent C boundary/randomized arithmetic, memory ordering, nested calls,
register hazards, large frames and stack-argument tests; it also cross-assembles
RV64IM with Clang when available. CTest and `tools/check.py` run these checks.
