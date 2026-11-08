# Unit — shared processor profile machinery

[Manuals](README.md) · [Library index](index.md)

`mulib-unit.h` supplies the common implementation for the [MPU](mulib-mpu.md),
[MCU](mulib-mcu.md) and [GPU](mulib-gpu.md) contracts. It owns a validated
[ArchML](mulib-archml.md) document, target width, byte order and family tag.
The family APIs share this representation and these query/codec functions;
their initialization functions select the appropriate family.

## Integration and lifecycle

Define `MULIB_IMPL_UNIT`, `MULIB_IMPL_ARCHML`, `MULIB_IMPL_PARSEC` and
`MULIB_IMPL_EMITTER` explicitly in one translation unit. There are no additional
native libraries, runtime profile files or host-architecture restrictions.
See the complete [example](../examples/modules/unit.c).

Zero initialize each context. `mulib_unit_init` loads an embedded builtin;
`mulib_unit_parse` loads a caller-supplied, length-delimited ArchML document.
Both reject an already initialized context and leave it unchanged on failure.
Source data is copied. `free` releases the document, invalidates borrowed views
and resets the context for reuse; NULL and repeated free are harmless. Do not
copy a live context or edit its public fields. Independent contexts and read-only
queries can be used concurrently; mutation/free requires caller synchronization.

| Public builtin name | Family | ArchML profile | Output |
|---|---|---|---|
| `amd64` | MPU | `amd64` | Little-endian machine code |
| `aarch64` | MPU | `arm64` | Little-endian machine code |
| `riscv64` | MPU | `riscv64` | Little-endian machine code |
| `avr32` | MCU | `avr32` | Big-endian AVR32A machine code |
| `xtensa` | MCU | `xtensa` | Little-endian 24-bit core instructions |
| `amdgpu` | GPU | `amdgpu` | Little-endian gfx600 SALU instructions |
| `nvptx` | GPU | `nvptx` | Selected PTX text operations |

Builtin names are exact and case-sensitive. `arm64` is the embedded document
name, not a second initialization spelling. A wrong builtin family or unknown
name returns `MULIB_ARCHML_MISMATCH`.

## Results, queries and codecs

All result-bearing operations use `mulib_archml_result`: **OK is 1**; INVALID,
SYNTAX, SCHEMA, LIMIT, NOMEM, IO, RANGE and MISMATCH are negative. Initialization
and emission refresh the optional `mulib_archml_error_t`; success clears its
message. Parsing diagnostics retain source locations, while wrapper-level errors
use offset zero and line/column one. Target-specific `init`/`init_ex` conveniences
instead return Boolean 1/0. The legacy AMD64 typed `emit` retains success **0**.

`document` borrows the complete immutable ArchML document. `find(kind,name)`
borrows an exact named record or returns NULL. Use ArchML `count`/`at` on the
document to enumerate forms, operations, ABIs, features, layouts or registers.
`register_info` returns 1/0 and exposes aliases, storage, width and encoding.
`enabled` returns 1 for satisfied explicit feature dependencies, 0 for disabled,
or -1 for an invalid query. It checks instructions and text operations. A feature
query does not probe host hardware or operating-system state.

`encode` accepts a **concrete form name** and numeric `uint64_t` operands in
profile order. Registers use encoding IDs; signed immediates use two's-complement
bit patterns, e.g. `(uint64_t)-8`. Scaled displacements are passed in bytes, not
encoded field units. ArchML enforces field width, scaling and encoded register
constraints. Feature, execution mode, privilege, ABI and memory legality remain
explicit consumer checks. Inspect the profile before selecting a form.

`decode` requires the concrete form name, validates a prefix and returns its
consumed byte count; trailing instructions are permitted. It is not an automatic
disassembler. Unknown forms and text-only operations return MISMATCH. All codec
outputs, including lengths and decoded operands, remain unchanged on failure.
NULL operand arrays require count zero; output buffers must not overlap operand
arrays, context storage or length outputs. ArchML limits are 16 operands and
32 encoded bytes per instruction. Encoding/decoding allocates nothing.

`emit` stages the encoding and appends it to a caller-owned emitter. The emitter
must have valid storage/size/capacity and match the target byte order. Failure
preserves every byte and its size. No automatic alignment, labels, relocation
processing, executable-memory allocation or instruction execution occurs.
Branch operands are already-resolved byte displacements with the profile's
specified PC base. NVPTX text rendering belongs to the GPU contract.

## Extension and resource limits

`parse` accepts future architecture names without modifying a builtin whitelist.
The caller explicitly assigns the family; this is classification, not hardware
certification. A profile must pass ArchML validation and declare a target width
in 1..64 and little/big endian. For a new architecture, provide its forms and
metadata in ArchML, initialize through the family `parse` API, then use the same
queries and codecs. No function-table implementation or copied ISA table is
required. A new concrete wrapper can later provide a builtin convenience.

Custom parsing accepts ArchML options: defaults are 16 MiB source, 1,000,000
nodes, nesting depth 128 and 100,000,000 work units. Initialization allocates an
owned parse tree; builtins additionally allocate a temporary source buffer.
Allocation failure leaves no live context. Lookup uses ArchML's indexed record
search; emitted instructions have bounded stack scratch storage.

The embedded snapshot is generated from the reviewed
[profiles](../library/archml/README.md) by `python3 tools/embed_units.py`.
After changing profile sources run that command; `--check`, CTest and
`tools/check.py` detect stale embedded data. The snapshot preserves profile
coverage and provenance; it adds no instruction encodings of its own.

Evidence: [processor regressions](../tests/test_units.c) cover golden encodings,
roundtrips, malformed inputs, ownership, output preservation, family rejection,
custom profiles and injected allocation failures. The existing ArchML suite
tests the underlying profile encodings against independent fixtures.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-unit.h`](../include/mulib-unit.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-unit.h
 * @brief Shared owned architecture profiles for MPU, MCU and GPU contracts.
 * Define MULIB_IMPL_UNIT, ARCHML, PARSEC and EMITTER explicitly once.
 * Zero initialize contexts; never copy ownership or modify their fields.
 * Builtins are generated from library/archml; no runtime files are required.
 */
#ifndef MULIB_UNIT_H
#define MULIB_UNIT_H
#include "mulib-archml.h"
#include "mulib-emitter.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_unit_family {
  MULIB_UNIT_MPU = 1, MULIB_UNIT_MCU, MULIB_UNIT_GPU
} mulib_unit_family;
/** Owned, immutable after initialization. Queries borrow from document. */
typedef struct mulib_unit {
  mulib_archml_t *document;
  mulib_unit_family family;
  mulib_emitter_order order;
  unsigned bits;
} mulib_unit_t;
/** Load a builtin by public name (aarch64 maps to ArchML arm64). Success is
 * MULIB_ARCHML_OK (1). Failure leaves context unchanged. Rejects live contexts,
 * unknown names and family mismatches. Error is optional and always refreshed. */
mulib_archml_result mulib_unit_init (mulib_unit_t *, mulib_unit_family,
    const char *name, mulib_archml_error_t *);
/** Load a future/custom profile into the explicitly selected family. Source is
 * copied; ArchML limits apply. Names need not belong to the builtin inventory.
 * Target width must be 1..64. This assigns a family, not hardware certification. */
mulib_archml_result mulib_unit_parse (mulib_unit_t *, mulib_unit_family,
    const char *, size_t, const mulib_archml_options_t *, mulib_archml_error_t *);
/** Release ownership, clear fields; NULL and repeated free are harmless. */
void mulib_unit_free (mulib_unit_t *);
/** Borrow the profile, or NULL for a NULL/uninitialized context. */
const mulib_archml_t *mulib_unit_document (const mulib_unit_t *);
/** Exact record lookup. Borrowed result survives until free. */
const mulib_archml_node_t *mulib_unit_find (const mulib_unit_t *,
    const char *kind, const char *name);
/** Resolve register aliases, encoding and overlapping-storage metadata. */
int mulib_unit_register_info (const mulib_unit_t *, const char *,
    mulib_archml_register_t *);
/** Check an instruction/operation's explicit feature set; 1/0/-1 as ArchML. */
int mulib_unit_enabled (const mulib_unit_t *, const char *name,
    const char *const *features, size_t count);
/** Encode a concrete form with numeric operands in profile order. Signed
 * operands are uint64 two's-complement patterns, displacements are in bytes.
 * Features/modes are checked separately. All outputs unchanged on failure. */
mulib_archml_result mulib_unit_encode (const mulib_unit_t *, const char *,
    const uint64_t *, size_t, void *, size_t, size_t *);
/** Decode a named concrete form; no instruction auto-detection. Transactional. */
mulib_archml_result mulib_unit_decode (const mulib_unit_t *, const char *,
    const void *, size_t, uint64_t *, size_t, size_t *);
/** Append one encoded form. Requires matching target byte order; no automatic
 * alignment, labels or relocations. Failure preserves emitter bytes and size. */
mulib_archml_result mulib_unit_emit (const mulib_unit_t *, mulib_emitter_t *,
    const char *, const uint64_t *, size_t, mulib_archml_error_t *);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-unit.h`](../include/mulib-unit.h) |
| Implementation switch | `MULIB_IMPL_UNIT` |
| Repository-local includes | [mulib-archml](mulib-archml.md), [mulib-emitter](mulib-emitter.md), [`include/mulib-unit-data.inc`](../include/mulib-unit-data.inc) |
| Complete example | [`examples/modules/unit.c`](../examples/modules/unit.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/unit.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
