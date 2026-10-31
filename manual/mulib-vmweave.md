# mulib-vmweave

Fault-preserving MIR optimization and a resumable typed register VM. The original
SSA optimizer and the control-flow compiler/executor have separate entry points.

## Integration and API

Include `mulib-vmweave.h`. Explicitly define `MULIB_IMPL_MIR` and
`MULIB_IMPL_VMWEAVE`, `MULIB_IMPL_EGRAPH`, and `MULIB_IMPL_REWRITE` once.
The optimizer itself does not require minivm; the
standalone example also implements MINIVM and CMACHINE to execute its result.

```c
mulib_mir_buffer_t output = {0};
mulib_mir_diagnostic_t diagnostic;
mulib_mir_status_t status = mulib_vmweave_optimize(&input, limit, &output, &diagnostic);
```

Input is borrowed and must remain immutable during the call. Output must be
empty and cannot alias input. Success transfers a new owned MIR buffer to the
caller; borrow it with `mulib_mir_view` and release with `mulib_mir_buffer_free`.
Input is never changed. Failure preserves empty output and releases scratch.
All diagnostics/status descriptions are from MIR. The returned record index
on success is not meaningful as an error location.

## Exact transformations

The entire input is first validated against `limit`, using the language in
`manual/mulib-mir.md`. All its opcodes are supported. A single forward pass:

1. Replaces uses of COPY records with their original sources.
2. Evaluates operations with known constant operands using
   `mulib_mir_evaluate`; successful evaluation becomes a typed CONST.
3. Leaves a faulting constant operation intact instead of treating a compiler
   error as execution, introducing undefined C arithmetic, or hiding the fault.

A reverse liveness pass then removes unnecessary CONST, COPY and nontrapping
comparisons/NOT, and rewrites surviving value IDs in a forward compaction pass.
All ADD/SUB/MUL/DIV/MOD/NEG records that were not successfully folded remain
live, even when their values are unused. Their dependencies remain live too.
PARAM records are retained, including unused Boolean parameter checks and the
unchanged exact argument-count contract. RETURN and its dependencies remain.
Surviving records retain their relative order, preserving first runtime fault.

For example, `6*7` becomes `CONST 42`, but unused `INT64_MAX+1` remains an ADD
and still faults. Neither `x*0 -> 0` nor reassociation is performed: those can
hide existing faults or introduce overflow. There is no global value numbering,
loop transformation, branch elimination, or aggressive algebraic optimizer.

## Limits and failures

Zero limits are actual zeros. Empty/malformed/cyclic MIR is rejected by common
validation. `limit` bounds input record count, and hence every scratch array:
two record buffers, one `size_t` mapping per input record, and one liveness byte
per input record. Each multiplication is checked. The output reserves at most
the input record count and does not grow. Allocation failures return NO_MEMORY;
representability/record bounds return RESOURCE_LIMIT. Work and memory are O(n).
No allocator injection interface is provided; normal C malloc/free ownership
applies. There is no retained global or borrowed scratch state.

## Runnable example

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/vmweave.c -lm -o /tmp/vmweave
/tmp/vmweave
```

The assert example optimizes a four-record product to two records, lowers that
result with CMACHINE, and executes it on MINIVM to 42. Tests include dead
overflow/division-by-zero preservation and equivalence on parameterized input.

## Control-flow compiler and VM

`mulib_vmweave_compile(source, limit, optimize, &image, diagnostic)` validates and
deep-copies a `mulib_mir_flow_program_t`, including its register types and all
parameter/host signatures. Initialize `image` to NULL. No source arrays are
retained. Failure leaves the output NULL. Release it with
`mulib_vmweave_program_destroy`; NULL is harmless. Borrow the resulting program
with `mulib_vmweave_program_view` and keep its owner alive until all frames using
it have been freed.

When `optimize != 0`, local constant propagation/folding operates within basic
blocks. Every original branch target, conditional fallthrough, and instruction
after a terminator starts a block. Constants never leak across joins or loop
backedges. A successful constant scalar computation becomes CONST; a known
BRANCH becomes JUMP. Faulting expressions and all host calls remain executable.
Floating computations/conversions remain runtime operations so compilation does
not change the floating environment or depend on the compile-time rounding mode.
Record indices stay stable, preserving branch targets, source mappings, and
instruction-budget behavior. There is no speculative execution of callbacks.
Optimization scratch is one typed value and one known-value byte per register,
plus one block-boundary byte per instruction.

Create a frame with `mulib_mir_frame_init`, then call:

```c
mulib_mir_status_t status = mulib_vmweave_run(&frame, instruction_budget);
```

This runs the register program, including loops, floating arithmetic, bitwise
operations and runtime calls. It returns FINISHED, STEP_LIMIT, SUSPENDED or a
fault, using the contracts in [mulib-mir](mulib-mir.md). Each run supplies a fresh
additional budget; zero executes nothing. Complete a suspended CALL with
`mulib_mir_frame_resume` before running again. Finished/faulted states persist.
Objects require external synchronization; independent frames share immutable
images without VM-global mutable state. Link `-lm` where required by MIR.

The control-flow optimizer also builds bounded e-graphs and applies
`mulib-rewrite` equality saturation for typed integer identities: `x+0`, `x-0`,
`x*1`, wrapping equivalents, and bitwise OR/XOR/shifts by zero. Minimum-cost
extraction replaces an operation with COPY only when the extracted class is its
input value. Earlier operand computations and their effects remain in place.
Floating arithmetic and dynamic host calls are excluded. Each candidate uses
at most eight enodes, ten rules, three saturation iterations and bounded
matching/extraction work. Graph allocation failures propagate as NO_MEMORY.
