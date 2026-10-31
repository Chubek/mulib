# mulib-mir

Typed record IR shared by the compiler backends. The original SSA interface is
joined by a typed register/control-flow dialect for VM, AOT and native execution.

## Integration and ownership

Include `mulib-mir.h`. Define `MULIB_IMPL_MIR` in exactly one translation unit.
There are no MuLIB implementation dependencies. Link the platform math library
for floating remainder (`-lm` where required). Declarations have C linkage in C++.

`mulib_mir_program_t` borrows immutable `code[count]`. Value IDs are record
indices, not pointers. Its lifetime must cover validation and any transformation.
It owns nothing. `parameter_count` is the exact argument-array length expected
by backends, including unused parameters.

`mulib_mir_buffer_t` owns a fixed-capacity record allocation. Zero initialize it,
call `mulib_mir_buffer_init(&b, capacity, parameters)`, append copied records with
`mulib_mir_append`, borrow a descriptor with `mulib_mir_view`, then call
`mulib_mir_buffer_free`. Do not copy a live owner, reinitialize it, or change its
capacity/pointer fields. Free zeroes the object and accepts NULL. Appending does
not validate; a partially built program is not yet executable. Capacity zero is
valid, but cannot accept an append. All allocations use the C allocator.

## Original SSA language

Every record has `opcode`, declared result `type`, operands `a`, `b`, and signed
`immediate`. Unused operands must equal `MULIB_MIR_NONE` (`SIZE_MAX`). Only CONST
and PARAM may have nonzero immediate. Every used operand must reference an
**earlier** record: forward references, self references, and cycles are rejected.

| Opcode | Operands / immediate | Result |
|---|---|---|
| CONST | immediate literal | I64 or BOOL; BOOL literal is exactly 0 or 1 |
| PARAM | immediate argument index, nonnegative and below parameter_count | I64 or BOOL |
| COPY | a | same type as a |
| ADD SUB MUL DIV MOD | a, b both I64 | I64 |
| NEG | a I64 | I64 |
| EQ | a, b with equal I64 or BOOL types | BOOL |
| LT LE | a, b I64 | BOOL |
| NOT | a BOOL | BOOL |
| RETURN | a I64 or BOOL | VOID |

There must be exactly one RETURN, at the final record. Nothing else is VOID.
No branches, loops, phi nodes, addresses, loads/stores, calls, floating-point,
implicit conversions, or external side effects are supported. Unsupported
opcodes/encodings produce diagnostics rather than disappearing.

Integer semantics are signed 64-bit with explicit overflow faults. DIV/MOD use
C11 truncation toward zero. Both `INT64_MIN / -1` and `INT64_MIN % -1` fault;
division by zero is distinct. NEG of `INT64_MIN` faults. BOOL values are canonical
0/1. Backends validate all Boolean PARAM arguments before arithmetic executes.
Records otherwise execute in their original order, including dead arithmetic:
an unused overflowing ADD still faults.

## Validation and diagnostics

`mulib_mir_validate(program, record_limit, diagnostic)` is allocation-free and
linear. It checks the entire input, not only returned dependencies. Zero records
are INVALID. A record count above the explicit limit, or one whose byte size
cannot be represented, returns RESOURCE_LIMIT. On failure, `diagnostic.record`
identifies the first offending record (zero for descriptor/limit failures).
The record index on success is not an error location and should be ignored.
`mulib_mir_status_string` returns static, non-owned diagnostic text.

Statuses distinguish INVALID descriptors/immediates, BAD_OPCODE,
BAD_REFERENCE (including cycles), BAD_TYPE, BAD_RETURN, RESOURCE_LIMIT,
NO_MEMORY, OVERFLOW and DIVIDE_BY_ZERO. The last two are evaluation faults;
validation does not execute arithmetic.

`mulib_mir_arity` returns 0/1/2, or -1 for an invalid opcode.
`mulib_mir_evaluate(op,a,b,immediate,&value)` is a checked scalar evaluator for
CONST, COPY, arithmetic, and comparisons/NOT. PARAM and RETURN are invalid in
this API. It does not validate operand types; first validate typed records.
Failure leaves `value` unchanged. There is no implicit error-global state.

## Bounds and composition

Builder allocation is exactly `capacity * sizeof(mulib_mir_instruction_t)`;
multiplication is checked before allocation. Appending to a full builder returns
RESOURCE_LIMIT without changing it. NO_MEMORY also preserves an empty builder.
Limits of zero mean zero, not unlimited. Callers must provide valid readable
arrays; C descriptors cannot establish the physical size of an arbitrary pointer.

`cmachine` lowers every supported opcode to checked `minivm`. `vmweave` returns
another owned MIR buffer with the same argument contract. `ccweave` emits an
independent C11 implementation of the same semantics. Generic e-graphs and
selectors can use MIR opcode numbers, but their clients must explicitly map
operands/types and preserve fault ordering; they are not silently typed MIR.

## Runnable example

`examples/modules/mir.c` constructs and validates a literal/return pair, then
asserts rejection of a self reference:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/mir.c -lm -o /tmp/mir
/tmp/mir
```

`tests/test_compiler_ir.c` exercises malformed types/references/opcodes, empty
input, capacities, arithmetic boundaries, and intermodule execution equivalence.

## Register/control-flow language

`mulib_mir_flow_program_t` borrows immutable instructions, a type for every
register, a function signature, and host-call signatures. Register types are
`I64`, canonical `BOOL`, C `double` (`F64`), or opaque unsigned 64-bit `WORD`.
The dialect uses mutable registers; values can cross loops and joins. Every
register starts at its type's zero value. Frontends must implement their own
language's lexical scopes, definite assignment, object lifetimes, and dynamic
type rules. A WORD can hold an index into a frontend-managed object table.

Each instruction has `opcode`, `destination`, `a`, `b`, `target`, `immediate`.
Unused indices are `MULIB_MIR_NONE`, and unused `immediate.word` is zero.

| `MULIB_MIR_FLOW_` opcode | Meaning |
|---|---|
| CONST | Copy typed immediate to destination |
| PARAM | Copy parameter indexed by a to destination |
| COPY | Copy a to same-typed destination |
| ADD SUB MUL DIV MOD NEG | I64 checked arithmetic, or F64 arithmetic; NEG uses only a |
| EQ LT LE | Same-typed comparisons producing BOOL; BOOL supports EQ only |
| NOT | BOOL logical negation |
| WRAP_ADD WRAP_SUB WRAP_MUL | I64 or WORD modulo-2^64 arithmetic |
| AND OR XOR SHL SHR | I64 or WORD bit operations; shifts are logical, and counts outside 0–63 produce zero |
| I64_F64 | Convert I64 to F64, with host double rounding |
| F64_I64 | Truncate toward zero; NaN, infinity, and values outside [-2^63,2^63) fault |
| JUMP | Unconditional absolute target |
| BRANCH | Jump to target if Boolean a is true, otherwise fall through |
| CALL | Call host target with contiguous registers [a,a+b), writing one typed result |
| RETURN | Return a, whose type must equal the function signature's result |

I64 division/remainder retain the original C truncating and overflow-faulting
semantics. F64 operations preserve the host's floating semantics, including
infinities, NaNs and signed zero; MOD uses `fmod`. WORD comparisons are unsigned.
Wrapping operations never evaluate overflowing signed C arithmetic. A negative
I64 shift count is outside 0–63 and produces zero; a Lua frontend must reverse
the shift direction itself to implement Lua's different rule.

`mulib_mir_flow_validate` validates every record, including unreachable records,
operand and result types, host arities/types, parameter references, and branch
targets. The final record must be JUMP or RETURN; multiple RETURNs and backward
edges are valid. Loops need not terminate. `limit` bounds each descriptor array,
function parameter count, and aggregate host parameter count. Zero is a real
limit. Allocation-size overflow is checked. Diagnostics identify a record for
instruction errors; for descriptor/type-array errors the index belongs to that
array. This validator has no allocation or execution side effects.

### Execution frames and suspension

Zero-initialize a `mulib_mir_frame_t`, then call `mulib_mir_frame_init` with a
validated program, arguments, callback and user pointer. Initialization also
validates the program, copies arguments, and allocates zeroed register storage.
Boolean arguments are checked before execution. The program must remain alive
and immutable until `mulib_mir_frame_free`; frames must not be copied. Free is
NULL-safe and resets the frame. A frame cannot be freed/reentered from its host
callback. Other frames can execute recursively or share immutable programs.

`mulib_mir_frame_step` consumes one unit of `remaining` and increments `steps`.
Faults count as instructions, but budget exhaustion does not. It returns:

- OK after a normal instruction;
- FINISHED after RETURN, with `frame.result` and the RETURN's `pc` retained;
- STEP_LIMIT before executing when `remaining == 0`;
- SUSPENDED if a host callback asks to suspend;
- an arithmetic fault, HOST_ERROR, or a resource error otherwise.

Faults preserve the destination and `pc`. Previously executed instructions and
host side effects persist. Repeated calls on finished, faulted or suspended
frames return the existing status. STEP_LIMIT is resumable after replenishing
the budget. Lifetime instruction-counter overflow returns RESOURCE_LIMIT.

A callback receives a borrowed argument slice, must not modify the frame or
program, and returns OK with a typed result, SUSPENDED, or a failure (reported
as HOST_ERROR). Invalid Boolean callback results are HOST_ERROR. On suspension,
call `mulib_mir_frame_resume(frame,result)` to complete that exact CALL without
repeating its side effects. Invalid Boolean resume values leave the frame
suspended. The caller supplies error handling/unwinding for its language.

### AOT output

`mulib_mir_flow_emit_c` writes C11 to a callback. The symbol must have a nonempty
`mulib_generated_` suffix and contain at most 127 ASCII identifier bytes. Floating
immediates require IEEE binary64 on the generator and generated target, with
matching integer/floating byte order; they are emitted as object bits to retain
signed zero and NaN payloads independently of the locale. A failed writer produces
IO_ERROR and can leave partial output; invalid IR is rejected before writing.

For symbol `mulib_generated_example`, output defines:

```c
const mulib_mir_flow_program_t *mulib_generated_example_program(void);
mulib_mir_status_t mulib_generated_example(mulib_mir_frame_t *frame);
```

Initialize a frame from the generated program getter, set `frame.remaining`,
then call the generated function. Program identity is checked. Output contains
owned static descriptors, direct C labels/branches, and shared MIR operation
helper calls; compile and link it with one MIR implementation. This baseline
AOT does not require a runtime source compiler or the VM dispatch loop.
Suspension and budget exhaustion can resume at any generated instruction.

`tests/test_mir_flow.c` checks VM/native equivalence, loops, ownership, suspended
host calls, checked/wrapping arithmetic, floating boundaries and malformed IR.
Its `--emit` mode writes an AOT regression translation unit for compilation.
