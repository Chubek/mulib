# ArchML compiler profiles

Load these version-1 S-expression descriptions using `mulib_archml_load`.
The requested spelling **`risv.aml`** describes **RV64 RISC-V**, not a separate ISA.

| File | Default compiler profile | Concrete forms |
|---|---|---:|
| `amd64.aml` | Long-mode little-endian, SysV AMD64, integer + SSE/SSE2 | 124 |
| `arm64.aml` | A64 little-endian EL0, AAPCS64, integer + scalar FP/selected NEON | 80 |
| `risv.aml` | RV64 little-endian LP64D, I/M/A plus selected D/C and FENCE.I | 92 |

Each includes register storage/aliases, classes and allocation restrictions,
concrete bit encodings, structured P-code, memory/atomic/exception semantics,
vectors/predicates, features/modes, types/legalization, instruction selection and
scheduling metadata, rewrites, reachability/relocations, assembly syntax, CPU
extension descriptions and SMT proof obligations. ABI and object-layout records
provide additional compiler integration data. Microarchitecture costs are
conservative generic estimates, not measured silicon timing guarantees.

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
[`tools/generate_archml.py`](../../tools/generate_archml.py); use `--check` in CI.
Tests load these actual files, roundtrip every form, and compare representative
forms with independent LLVM MC encodings.
