# mulib-lowopt

Conservative low-level optimization of [native MIR](mulib-mir.md). It follows
wrapping machine-word semantics, not the older checked SSA dialect.

## Integration and API

Define `MULIB_IMPL_MIR` and `MULIB_IMPL_LOWOPT` before including `mulib-lowopt.h`
in one translation unit. Link `-lm` where required by MIR. Public declarations
have C linkage.

```c
mulib_mir_native_buffer_t optimized = {0};
mulib_lowopt_stats_t stats;
mulib_mir_status_t status = mulib_lowopt_run(
    &program, 65536, &optimized, &stats, &diagnostic);
/* Read mulib_mir_native_view(&optimized), then: */
mulib_mir_native_free(&optimized);
```

The input is validated and deep-copied first. Successful output owns every array
and copied symbol name. Failure leaves the empty output and optional stats
unchanged. Zero-initialize owners, do not copy them, and release with the MIR
free function. Input arrays are never edited.

## Transformations

Each block starts with unknown register constants; values are never propagated
across joins without proof. The pass tracks assignments in their original order:

- constant COPY and binary arithmetic/bit operations/comparisons become CONST;
- addition/subtraction/OR/XOR by zero, multiplication by one, AND with all bits
  set, and shifts whose masked count is zero become COPY;
- same-register SUB/XOR/SLT/ULT become zero; same-register EQ becomes one;
- proven constant branches become explicit JUMPs;
- self COPY becomes NOP;
- repeated global use scans remove pure writes to registers that are never read.

Dead-write elimination is deliberately conservative with mutable registers: it
does not infer that a particular overwritten definition is dead just because
another assignment reaches a use. Reads include call argument slices. It does
not eliminate unreachable blocks, LOAD/STORE, or CALL, nor does it reorder any
instruction. Loads that might fault and external side effects therefore remain
observable. Arithmetic cannot fault in this wrapping dialect.

Removed instructions become NOPs. Record and block IDs remain stable, including
branch targets and diagnostics. Statistics count folds, simplifications and
removed records. The pass runs before selection/scheduling in MicroCC; it does
not edit an already selected instruction plan.

## Scalar evaluation and limits

`mulib_lowopt_evaluate(op,a,b,&result)` evaluates native binary operations with
unsigned C arithmetic to avoid signed overflow. ADD/SUB/MUL are modulo 2^64;
SHL/SHR/SAR mask counts with 63; SAR sign-extends; SLT interprets signed bits and
ULT unsigned bits. EQ returns 0/1. It returns one on success, zero for an
unsupported opcode or NULL result, preserving the result on failure.

`limit` bounds all native descriptor arrays; sizes are checked before allocation.
Work storage is proportional to registers plus the copied program. Repeated
dead-use scans may take quadratic time. NO_MEMORY and RESOURCE_LIMIT preserve
outputs. Zero is a real bound; input pointers must refer to readable arrays.

## Example and verification

```sh
cc -std=c11 -Iinclude examples/modules/lowopt.c -lm -o lowopt
./lowopt
```

The example folds `6 * 7` and removes its unused operand definitions. MicroCC
tests compare passes enabled/disabled through loop PHIs, native randomized
boundary operations, mutable hazards and effectful calls, and inject allocation
failures through the optimization pipeline.
