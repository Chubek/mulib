# ArchML compiler profiles

Load these version-1 S-expression descriptions using `mulib_archml_load`.
The canonical RV64 RISC-V filename is **`riscv64.aml`**.

| File | Default compiler profile | Concrete forms |
|---|---|---:|
| `amd64.aml` | Long-mode little-endian, SysV AMD64, integer + SSE/SSE2 | 124 |
| `arm64.aml` | A64 little-endian EL0, AAPCS64, integer + scalar FP/selected NEON | 80 |
| `riscv64.aml` | RV64 little-endian LP64D, I/M/A plus selected D/C and FENCE.I | 92 |
| `ppc64.aml` | Big-endian PowerPC64, selected non-recording integer forms | 23 |
| `mips.aml` | Big-endian pre-R6 MIPS32, selected integer forms and delayed branches | 29 |
| `avr32.aml` | Big-endian AVR32A, selected 16-bit forms; **not AVR8** | 9 |
| `wasm.aml` | WebAssembly MVP wasm32, selected stack operations and SLEB7 constants | 36 |

The AMD64, AArch64 and RV64 compiler profiles include register storage/aliases, classes and allocation restrictions,
concrete bit encodings, structured P-code, memory/atomic/exception semantics,
vectors/predicates, features/modes, types/legalization, instruction selection and
scheduling metadata, rewrites, reachability/relocations, assembly syntax, CPU
extension descriptions and SMT proof obligations. ABI and object-layout records
provide additional compiler integration data. Microarchitecture costs are
conservative generic estimates, not measured silicon timing guarantees.

The four newer profiles are deliberately smaller encoding/P-code subsets, not
full compiler-backend profiles. They make no ABI, tuning or exhaustive ISA claims.
MIPS branches explicitly execute a delay slot, unlike RISC-V branches. PPC64 uses
constant zero for an encoded r0 base in ADDI/ADDIS and supported memory forms,
but reads the actual r0 register in ordinary arithmetic/logical forms. AVR32
excludes PC operands and preserves the specified status flags. WebAssembly has
no invented physical registers: operations consume/produce typed stack values.
Its `i32.const-sleb7`/`i64.const-sleb7` forms accept only -64..63; the current
fixed-bit encoding schema cannot express general variable-length LEB128.

## Local source provenance

Reviewed facts come from `/mnt/lambda/ArchTk/infobank/` (paths below are relative
to that directory); regeneration does not require the corpus:

| Profile | Source files |
|---|---|
| RISC-V | `Capstone-Arch/RISCV/RISCVGenDisassemblerTables.inc`; `LLVM-Target/RISCV/RISCVInstrInfo.td` |
| PowerPC64 | `Capstone-Arch/PowerPC/PPCGenDisassemblerTables.inc`; `LLVM-Target/PowerPC/PPCInstrInfo.td`, `PPCInstr64Bit.td`, `PPCInstrFormats.td` |
| MIPS32 | `Capstone-Arch/Mips/MipsGenDisassemblerTables.inc`; `LLVM-Target/Mips/MipsInstrInfo.td`, `MipsInstrFormats.td` |
| AVR32A | `Ghidra-CPU/Atmel/data/languages/avr32a.slaspec` and its arithmetic, logic, data-transfer and system-control `.sinc` files |
| WebAssembly | `Capstone-Arch/WASM/WASMDisassembler.c`; `Binaryen-WASM/wasm-stack.cpp`, `wasm-validator.cpp` |

AVR32 is absent from this Capstone snapshot; its profile uses the surrounding
Ghidra corpus instead. Tables describe selected facts, not a wholesale conversion
of third-party source. LLVM MC independently checks the supported PowerPC64 and
MIPS forms; AVR32 and WebAssembly have source-derived byte fixtures instead.

## Coverage contract

An `instruction` record is one encodable form, with bounded operands and a P-code
expression. Unsupported addressing variants are not implied by the mnemonic.
Encoder support is intentionally explicit: the files do **not** contain every
instruction, privileged facility or extension of these very large architectures.
For example, AVX/AVX512, SVE/SVE2 and RVV have feature/state descriptions but are
marked metadata-only, and their encodings cannot be selected from these profiles.
RISC-V F/D coverage is partial even though LP64D ABI data is provided. Both x86
Intel and AT&T syntax contracts are present; there is no built-in text assembler.

Legalization records describe promotion, expansion, scalarization and target
runtime fallbacks. A compiler still implements ABI classification, feature/mode
selection, instruction selection, allocation, scheduling, object emission,
relocation handling, runtime linkage and semantic verification. Scalable operations
without an implemented backend must be diagnosed. These assets are a substantial
backend data foundation, not a claim of a complete production compiler target.

## Using the data

1. Select the target/ABI and an explicitly enabled feature set; validate OS state.
2. Legalize IR types and operations according to available forms and runtime ABI.
3. Match `selection` records; inspect `operands`, implicit uses/defs and P-code.
4. Allocate physical views using storage overlap, classes, ties and ABI constraints.
5. Schedule effects and dependencies with the selected tuning model.
6. Lay out code, check displacement scale/range, insert veneers and relocations.
7. Call the concrete encoder with architectural register numbers and byte-valued
   signed displacements, or translate assembly templates into the chosen dialect.
8. Apply machine rewrites only after discharging their flags/memory/trap guards.

The public schema and ownership contracts are in
[`manual/mulib-archml.md`](../../manual/mulib-archml.md).
The AML is generated deterministically from
[`tools/generate_archml.py`](../../tools/generate_archml.py) and
[`tools/archml_extra.py`](../../tools/archml_extra.py); use `--check` in CI.
Tests load these actual files, roundtrip every form, and compare representative
forms with independent LLVM MC encodings.
