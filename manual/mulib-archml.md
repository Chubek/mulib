# ArchML — architecture data for compiler backends

`mulib-archml.h` provides a versioned S-expression DSL, owned immutable documents,
validated cross-references, register maps, structured instruction P-code, and
transactional instruction encoding/decoding. The data model covers all sixteen
architecture-description areas; the supplied files declare **specific compiler
profiles and concrete instruction forms**, rather than claiming exhaustive ISA
coverage. See [profile coverage](../library/archml/README.md).

## Integration

Define `MULIB_IMPL_ARCHML` and `MULIB_IMPL_PARSEC` in one C11 translation unit.
No implementation switches are enabled implicitly. PARSEC's AST declarations
are included transitively; neither AST nor PEG implementations are needed.
Public declarations have C++ linkage guards. All allocation uses the C standard
library; the encoder/decoder allocate nothing. Independent immutable documents
and concurrent queries have no shared mutable state.

```c
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_ARCHML
#include "mulib-archml.h"

mulib_archml_t *architecture = NULL;
mulib_archml_error_t error;
if (mulib_archml_load("library/archml/riscv64.aml", NULL,
                      &architecture, &error) == MULIB_ARCHML_OK) {
    uint64_t operands[] = {10, 11, 12}; /* ADD a0,a1,a2 */
    unsigned char bytes[4];
    size_t length;
    mulib_archml_result result = mulib_archml_encode(
        architecture, "add", operands, 3, bytes, sizeof bytes, &length);
    /* On OK: 33 85 c5 00. */
    (void)result;
    mulib_archml_destroy(architecture);
}
```

Files are deployed separately; no working-directory search, environment lookup,
network fetch, shell execution or architecture auto-detection occurs. Paths are
explicit. The standalone [example](../examples/modules/archml.c) embeds a tiny
description and requires no deployed assets.

## Parsing, ownership, limits and diagnostics

`parse(source, length, options, &document, &error)` consumes exactly the supplied
bytes, including trailing whitespace/comments, and makes independent owned copies
of all atoms/strings. The source need not be NUL-terminated and can be released
afterward. `load(path, options, &document, &error)` reads with the same size limit,
closes the file on every path, and invokes the same parser and validator.

Success is `MULIB_ARCHML_OK` (1). Errors distinguish INVALID arguments (-1),
SYNTAX (-2), SCHEMA (-3), LIMIT (-4), NOMEM (-5), IO (-6), operand RANGE (-7),
and decoder MISMATCH (-8). Negative results leave the caller's output document
unchanged. Do not overwrite an owned document on success; destroy it first.
`destroy(NULL)` is harmless. `error` is optional; its static message survives
document destruction. Offsets are zero-based source bytes, lines/columns are
one-based byte positions. A resource/file failure may identify offset zero.

Zero option fields select:

| Option | Default | Meaning |
|---|---:|---|
| `max_source` | 16 MiB | Source bytes, including comments/docstrings |
| `max_nodes` | 1,000,000 | Captured atoms, strings and lists |
| `max_depth` | 128 | Nested S-expression lists |
| `max_work` | 100,000,000 | Parser work budget and a separate validation budget |

PARSEC executes on an explicit stack. Its bounded entry point charges parser
entries and literal comparisons; ArchML also bounds semantic-validation visits
and duplicate/dependency checks. Limits are deterministic resource policies, not
wall-clock deadlines. Document construction and destruction use flat arrays,
without recursive tree cleanup. Encoding lengths/bit indices, integer conversions,
array allocation sizes and register storage slices use checked arithmetic.

## DSL syntax

```text
document := whitespace (archml 1 name record*) whitespace EOF
record   := (kind name property*)
property := (key value*)
value    := atom | string | (value*)
```

Atoms are nonempty printable ASCII excluding whitespace, parentheses, quotes,
backslash and semicolon. Strings are double-quoted byte strings and support
`\"`, `\\`, `\n`, `\r`, `\t`; raw NUL/control bytes and unknown escapes are
invalid. Non-ASCII string bytes are preserved, without Unicode normalization.
Semicolon comments extend to newline or EOF. Lists/strings delimit adjacent
tokens; ordinary atoms require a delimiter. There are no macros, implicit
includes, expressions evaluated during loading, or interpolation in strings.

Unsigned numeric API conversion accepts decimal or `0x` hexadecimal, with
checked uint64 overflow; no sign or whitespace. Signed conversion additionally
accepts a leading `-`, including `-9223372036854775808`. Conversion failure leaves
the output unchanged. Strings containing numbers can also be converted.

Exactly one `target` record is required. Names are case-sensitive; `(kind,name)`
is a unique key. Each top-level record's named properties must be unique.
Standard record kinds are listed below; vendor-defined kinds begin with `x-`.
Properties are extensible so architectural and microarchitectural facts can be
added without changing the C ABI. A `(ref kind name)` anywhere in the document
must resolve, with exactly those three children; use it for relationships that
should be checked rather than storing an unchecked symbolic name.

## Record vocabulary

| Records | Compiler purpose and principal properties |
|---|---|
| `target`, `coverage` | Version, target width/endian/triple, default feature set and ABI, precise coverage contract and references |
| `storage`, `register`, `regclass` | Physical storage, overlapping views, encoding/DWARF IDs, write policy, register classes, reserved registers, spill alignment, scalable groups |
| `instruction`, `operation`, `pcode-op` | Concrete binary forms or non-binary text/IR operations, operands, semantic expression, explicit/implicit uses/defs, modes/features, effects, assembly template and scheduling class |
| `mode`, `microarch` | Privilege, operand/address width, execution-state constraints, issue width, hazards and named tuning model |
| `memory`, `addressing` | Memory model/endian/address width, alignment, access atomicity, address formulas and legal offset forms |
| `atomic` | Acquire/release/seq-cst mappings, fences, RMW/LL-SC reservations, alignment and forward-progress conditions |
| `exception` | Trap classes, precise state, restart PC, fault ordering, integer/FP exception distinctions |
| `vector`, `predication` | Lane shapes, scalable vector/predicate state, mask/tail policy, condition-code interpretation and inversion |
| `feature`, `extension` | Feature dependency DAG, OS state prerequisites, extension semantics and concrete/metadata-only coverage |
| `type`, `legalize` | Type size/alignment, native operations, promotion, expansion, scalarization and runtime-helper recipes |
| `selection`, `schedule`, `resource` | Selection patterns/costs, instruction refs, latency, throughput, resource occupancy and barriers |
| `rewrite` | Match/replacement or recipe, alias/flag/fault guards and proof obligations |
| `layout`, `relocation` | Instruction alignment, displacement scale/PC base/reach, literal pools, veneers, relaxation and ELF relocation formulas |
| `syntax` | Assembler dialect, operand order, registers/immediates, memory syntax and relocation spelling |
| `abi` | Argument/result classes, register preservation, stack/red-zone/home-space layout, aggregates, varargs and unwinding requirements |
| `smt` | Bitvector/array/FP theory, definedness, undefined state, memory/effect equivalence, proof and optimization objectives |

These records are inspectable compiler input. Loading does not construct an
instruction selector, allocator, scheduler, object writer, ABI classifier or SMT
solver. Some high-level rules are deliberately expressed as documentation and
recipes; consumers implement those algorithms and check the selected profile's
coverage. Unknown extension properties are retained, not silently interpreted.

### Registers and allocation

```lisp
(storage rax (bits 64))
(register eax (bits 32) (storage rax) (offset 0)
  (encoding 0) (write zero-extend))
```

Register `bits`, `offset`, `encoding`, `storage`, and `write` are mandatory.
The storage must exist; the nonempty slice must fit. Policies are `replace`,
`preserve`, `zero-extend`, and `discard`. Per-instruction vector semantics can
refine these policies (legacy SSE versus VEX, for example). Storage widths marked
scalable represent a maximum container; consumers must use architectural VL,
VLEN/SEW/LMUL, not allocate that maximum as a fixed vector type automatically.

`register_info(document,name,&info)` returns a borrowed typed view; missing DWARF
numbers are `UINT64_MAX`. `register_overlap` returns 1 for intersecting slices in
the same storage, 0 otherwise, -1 for unknown names. Aliases such as `a0`/`x10`
share storage. A class has a nonempty `members` list of register references of
the declared width. `reserved` registers are unavailable to allocation but can
still be explicitly encoded (stack-pointer operands, for example).

Operand `access` exposes ties through `read-write` as well as `read` and `write`.
Instruction `implicit-use`/`implicit-def` lists expose flags, stack pointers and
special registers. Calling convention clobbers, scratch registers, spill slots,
early-clobber constraints and instruction-specific notes must also inform the
allocator. Register overlap alone is not an allocation algorithm.

### Features and modes

`(requires (ref feature ...))` is a conjunction. The parser rejects unresolved,
wrong-kind and cyclic requirements. `enabled(document,record,features,count)`
returns 1 only if every required feature and its transitive prerequisites are
explicitly listed. Missing prerequisites return 0; invalid arguments/names or
query resource exhaustion return -1. The query uses a bounded stack (128 pending
dependencies) and 100,000 visits. Feature names do not imply enabled kernel/OS
state. Mode, privilege, calling convention and memory-type checks remain explicit
consumer responsibilities. A metadata-only extension must not be selected as if
it supplied a concrete instruction encoder.

### Concrete operands and bit encodings

```lisp
(instruction add
  (requires (ref feature base))
  (operands
    (rd  (kind register) (bits 5) (signed false) (scale 1)
         (access write) (class (ref regclass GPR)))
    (rs1 (kind register) (bits 5) (signed false) (scale 1)
         (access read) (class (ref regclass GPR)))
    (rs2 (kind register) (bits 5) (signed false) (scale 1)
         (access read) (class (ref regclass GPR))))
  (encoding (base "33000000")
    (field rd 7 5 0) (field rs1 15 5 0) (field rs2 20 5 0))
  (pcode (write rd (bvadd 64 (read rs1) (read rs2)))))
```

Operand names are unique and ordered. An operand is a `register` or `immediate`,
has 1..64 encoded bits, explicit signedness, a power-of-two scale, and access mode.
Register operands additionally reference a register class and must be unsigned,
scale one. Optional `(exclude 5 6)` removes reserved operand values; the decoder
rejects them too. Values for excluded signed operands use their uint64 bit pattern.

`base` is a nonempty lowercase hex byte string, in **wire order**. A field is
`(field operand destination-bit bit-count source-bit)`. Destination bit zero is
the low bit of the first byte, independent of the target data endianness.
Source bits refer to the operand **after dividing by its scale**. This represents
RISC-V B/J scatter, A64 ADR split fields, and x86 REX/ModRM/SIB without special
architecture code. Fields must not overlap fixed one bits or other destination
fields. Each operand bit must appear exactly once, and all indices must fit.
At most 16 operands and 32 bytes are supported per concrete template.

`encode` takes operands in declaration order. Register operands use architectural
encoding numbers, not document indices or names. Signed values are uint64
two's-complement patterns; e.g. `(uint64_t)-8`. Scaled displacements are **bytes**,
not already-divided instruction units. The API rejects misalignment, range
overflow, excluded values and numbers absent from the referenced register class.
For an immediate of B encoded bits and scale S, unsigned range is
`0..(2^B-1)*S`; signed range is `-2^(B-1)*S..(2^(B-1)-1)*S` in S-sized steps.
There is no silent truncation or automatic relaxation.

`decode` matches a named form and returns operands plus consumed prefix length.
Short input is LIMIT; differing fixed bits/reserved operands are MISMATCH.
It does not choose among overlapping aliases, assemble text, infer features,
resolve symbols or select branch veneers. All outputs are transactional. Buffer
capacity failure also leaves bytes untouched. Caller operand arrays and byte
buffers must be valid and nonoverlapping.

### Non-binary operations (PTX and DXIL)

`operation` records describe forms that emit text or LLVM IR, not native bytes.
They require `(emission text)` or `(emission llvm-ir)`, an `operands` property,
and one validated `pcode` expression. An `encoding` property is forbidden.
For example:

```lisp
(operation add.s32
  (emission text)
  (operands
    (dst (kind value) (bits 32) (signed false) (scale 1) (access write))
    (lhs (kind value) (bits 32) (signed false) (scale 1) (access read))
    (rhs (kind value) (bits 32) (signed false) (scale 1) (access read)))
  (pcode (write dst (bvadd 32 (read lhs) (read rhs))))
  (assembly "add.s32 {dst}, {lhs}, {rhs};"))
```

This example assumes the usual `write`, `read`, and `bvadd` operator declarations.
`value` operands are typed virtual values with widths 1..64, no physical class,
unsigned encoding metadata and scale one; they are allowed only in `operation`
records. Operations can also use ordinary immediate/register operand descriptors.
The same operand uniqueness, references, access bindings, feature requirements
and P-code operator/arity checks apply as for binary instructions. This is not
full SSA/type or module validation. Immediate exclusions document consumer-side
constraints; there is no non-binary emission/operand-validation API.

Use the existing `find`, `at`, `count`, `property` and `enabled` APIs to inspect
operations. The binary encoder/decoder only look up `instruction` records: a
non-binary operation name returns `MULIB_ARCHML_INVALID`, preserving every output.
All existing function signatures, public structures and version-1 binary forms
are unchanged. No text assembler, PTX-to-SASS compiler or DXIL bitcode writer is
implicitly added.

### P-code semantic contract

`pcode` contains one S-expression. Every list operator must have a `pcode-op`
record with `(arity minimum maximum)` and `(semantics "...")`. The parser checks
operator existence/arity and named `read`/`write` operands and `reg`/`set-reg`
registers. It validates structure and bindings, **not a full semantic type proof**.
Consumers type-check widths, effect tokens, conditions and operator contracts
before translation to their IR or a proof engine.

The supplied operator vocabulary documents modular integer arithmetic, extension,
extraction/deposit, comparisons, register state, memory, control flow, fences,
atomics, flag updates, IEEE-754 and lane-wise vector operations. Width parameters
are explicit. Reads observe pre-state. `parallel` commits writes together after
fault checks; it does not serialize them left-to-right. Unwritten state is
preserved and ordinary instructions advance to `next-pc`. `pc` means the current
instruction, which differs from x86 relative-branch base `next-pc`.

Architecture-specific operators capture genuinely different behavior: A64
division by zero returns zero; RISC-V division returns all-one quotient; x86
division traps and uses double-width implicit dividends. Undefined flags become
fresh nondeterministic values; they are neither zero nor IR poison. FP includes
the environment, NaNs, rounding and sticky exceptions. Atomic operations carry
ordering and reservation semantics and cannot be optimized as ordinary loads.

No opcode operator is executed while loading. There is no hidden emulator or
solver. A backend can traverse the immutable P-code and lower declared operators
into its own typed effects or SMT encoding.

## Query API

`root`, `child`, `property`, `count`, `at`, and `find` return borrowed views.
`child(list,i)` is zero-indexed. `property(list,key)` finds an immediate child list
whose first atom equals key. `find(kind,name)` uses sorted O(log records) lookup;
`at(kind,index)` preserves source order. `count`/`at` scan records. List nodes have
NULL text; leaf text is decoded, NUL-terminated. `offset`/`length` describe the
original source span (including string quotes), not decoded string length.
`subtree_first` is a read-only implementation detail for flat postorder storage.
All views expire with the document. Do not mutate public node fields.

## Verification and maintenance

* `tests/test_parsec_hardening.c`: 20,000-level explicit-stack captures, limits,
  fatal-error backtracking and literal work bounds; existing PARSEC/PEG clients
  continue to use their unchanged API.
* `tests/test_archml.c`: real deployed profiles, all-form roundtrips, independent
  byte fixtures, truncation, references, cycles, malformed input, limits and views.
* `tests/test_archml_alloc.c`: fails every parse/tree/index/feature and PARSEC
  stack/capture allocation, requiring clean outputs and zero retained allocations.
* `tools/check_archml.py`: independent LLVM MC assembler comparisons, including
  high registers, SIB addressing, subword accesses, flags, FP, atomic order bits,
  ARM A32, gfx600 SALU, eBPF and Hexagon packets. Optional `spirv-as` comparisons
  check SPIR-V word order and ID serialization independently.
* `tests/fuzz_headers.c`: bounded hostile-input loading and ownership checks.

Edit `tools/generate_archml.py`, `tools/archml_extra.py` or
`tools/archml_targets.py`, run the generator to regenerate the sixteen checked-in
`.aml`/`.am` assets, then run `--check` to verify reproducibility. File extensions
are a deployment convention, not a parser restriction. Generator tables are reviewed
source data, not claims that complete ISA references have been mechanically
imported. Add new concrete forms together with assembler fixtures and semantic
contracts. Extend runtime coverage records whenever a feature becomes selectable.
