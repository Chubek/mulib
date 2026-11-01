# mulib-machine

Retargetable machine descriptions, costed MuBURG selection and standalone ELF
assembly emission for **x86-64 SysV** and **RV64IM LP64**.

## Integration

Define `MULIB_IMPL_MIR`, `MULIB_IMPL_MUBURG` and `MULIB_IMPL_MACHINE` in one
translation unit before including `mulib-machine.h`. Link `-lm` if needed by
MIR's other APIs. Declarations have C linkage. Target descriptors, grammars,
latencies and callbacks are borrowed immutable data; built-in getters return
static descriptors that require no cleanup.

## Target grammar and instruction selection

`mulib_machine_t` names a target and provides 64-bit word size, power-of-two
stack alignment, signed immediate width, a MuBURG rule array, parallel positive
latencies, an emitter callback and arbitrary immutable context. The currently
defined native MIR data model is word-sized; a 32-bit target must supply its own
legalization before using it, rather than changing `word_bits` to 32.

Two grammar goals are fixed: REG (0) and IMM (1). Operand leaf terminals are
REGISTER, IMMEDIATE (a known signed value fitting `signed_immediate_bits`) and
WIDE (a known value that does not fit). Root terminals are native MIR opcodes
with the native scalar arity. CALL arguments are an external register slice;
CALL itself has zero scalar children. Leaf instruction IDs are zero; root
instruction IDs are target-defined. A native root must produce the REG goal,
even when it is a terminator/effect rather than a value.

`mulib_machine_select(machine, instruction, known_mask, values, limit, out)`
builds a small tree containing the native operation and its operand leaves.
It uses MuBURG to find the minimum additive-cost cover. Known bits 0/1 describe
a/b and require values[0/1]. A selected IMM child becomes `immediate_mask` plus
literal bits in the result; a REG child is loaded from its virtual register.
Selection does not recursively inline producers, duplicate shared values or
discard effects. It assumes the caller's known values were proven at that exact
program point. MicroCC computes these facts independently for each block.

Costs describe the local grammar, not an absolute hardware optimum. Ties use
rule order; latencies are separate scheduling data. Built-ins offer register
forms and legal right-immediate alternatives. x86 classifies signed 32-bit
immediates; RV64 classifies signed 12-bit immediates. Wide constants retain
register forms. Shifts mask immediate counts before encoding. There are no
semantic-predicate or multi-terminal-root grammar rules in this interface.

Failure leaves selection unchanged. Invalid grammars/descriptors return INVALID,
missing covers UNSUPPORTED, cost/storage overflow RESOURCE_LIMIT and allocation
failure NO_MEMORY. `limit` bounds rules; selection needs at most three nodes and
six DP cells. `mulib_machine_validate` checks grammar metadata, arities, goals,
leaf IDs, latencies and array bounds.

## Built-in ABI and register handling

Both built-ins implement all currently defined native MIR operations. Each
virtual register has a stable eight-byte spill slot initialized to zero at entry.
Parameters have separate saved slots, so PARAM instructions remain correct even
after calls clobber incoming argument registers. Scratch registers never hold a
live value across an emitted operation. Calls and joins therefore need no
implicit register-allocation assumptions.

| Target | Scratch / ABI |
|---|---|
| `mulib_machine_x86_64_sysv()` | AT&T GNU syntax; RAX/RCX scratch, RBP frame; RDI, RSI, RDX, RCX, R8, R9 argument registers; RAX result |
| `mulib_machine_rv64im()` | GNU/LLVM RISC-V syntax; T0/T1/T2 scratch, S0 frame, RA saved; A0..A7 arguments, A0 result; M extension for MUL |

Additional arguments go on the stack. Outgoing areas and frames are rounded to
16-byte alignment. Callee-saved state used by generated code is preserved;
incoming parameter registers are captured before scratch work can clobber them.
RISC-V large offsets use synthesized addresses rather than truncating immediates.
RISC-V control transfers use local conditional skips and PC-relative AUIPC/JALR
(`tail`) sequences, so distant blocks do not overflow short branch/JAL ranges.
Frame/incoming/outgoing offsets are conservatively bounded to signed 32-bit
range and rejected before text emission. Stack storage/lifetime is supplied by
the execution environment; the compiler does not implement stack probing.

Calls are direct named, fixed-arity, one-word-result ABI calls. No floating-point,
aggregate, variadic, exception/unwind or indirect-call ABI is implemented. Raw
memory pointers must refer to valid aligned native-word storage. Memory ops are
ordinary non-atomic loads/stores. Generated functions need no helper runtime.

## Emission and extending targets

`mulib_machine_emit` validates the native program, target, symbol and block-local
order permutation, including the final terminator position. `selected` must be
an unmodified selection table for that exact program; `order` must preserve
semantic dependencies. The wrapper does not redo scheduling or reprove known
constants. Use MicroCC to construct these arrays together safely.

The callback receives the machine/context, native MIR, selections and order,
symbol, writer/user and diagnostic. It owns all register/ABI decisions and text
formatting. A writer returns nonzero on success. Direct emission can be partial
on error; MicroCC buffers it and publishes output only after success. A custom
target can replace grammar costs/encodings, latencies, register placement and
assembly syntax without modifying MicroCC. Descriptor data must stay immutable
through the call. There are no mutable global target registrations.
Reusing a built-in emitter requires its 16-byte alignment and an immediate
classification no wider than that ISA supports; other ABI metadata needs a
matching custom emitter and is diagnosed as UNSUPPORTED by the built-in.

Symbols must be ASCII identifiers of 1..127 bytes. Built-ins emit unique local
labels incorporating the symbol, ELF type/size directives and nonexecutable
GNU-stack metadata. Multiple differently named functions can be concatenated.
Use GNU as/ld, LLVM or a compiler driver to assemble/link them. The current
MuLIB assembler has a typed register/immediate subset rather than the required
text/memory-operand frontend.

## Example and checks

```sh
cc -std=c11 -Iinclude examples/modules/machine.c -lm -o machine
./machine
```

The example selects immediate ADD on both targets. MicroCC tests change rule
costs to force another cover, reject unavailable covers and malformed targets,
execute x86-64 assembly with independent arithmetic/ABI checks, and
cross-assemble RV64IM output when Clang is available.
