# MPU — microprocessor contract

[Manuals](README.md) · [Library index](index.md)

`mulib-mpu.h` defines the common owned architecture and code-emission contract
for [AMD64](mulib-amd64.md), [AArch64](mulib-aarch64.md),
[RISC-V64](mulib-riscv64.md) and future microprocessors. It supplies lifecycle,
metadata lookup, register aliases, feature queries, concrete-form encoding,
named-form decoding and transactional emitter append.

## Integration and ownership

Define `MULIB_IMPL_MPU`, `MULIB_IMPL_UNIT`, `MULIB_IMPL_ARCHML`,
`MULIB_IMPL_PARSEC` and `MULIB_IMPL_EMITTER`. See the
[runnable example](../examples/modules/mpu.c). No runtime assets are required.

`mulib_mpu_t` is the shared unit representation. Zero initialize, call
`mulib_mpu_init(&context, name, &error)`, then `mulib_mpu_free`. Names are
`amd64`, `aarch64` and `riscv64`. Success is `MULIB_ARCHML_OK` (1), errors are
negative. A live context is rejected; failed initialization preserves it.
Borrowed query results expire at free. Do not copy ownership. NULL and repeated
free are harmless; a freed context can be initialized again.

## Shared API and extension

The `document`, `find`, `register_info`, `enabled`, `encode`, `decode` and `emit`
names are public aliases of the corresponding unit functions, with identical
signatures and behavior. See the [shared contract](mulib-unit.md) for result
codes, numeric operands, endian checks, concurrency and resource limits.
For example, `mulib_mpu_emit(&context, &image, "add64", values, 3, &error)`
uses an AArch64 concrete form. Feature availability is queried separately.

`mulib_mpu_parse` accepts an arbitrary validated ArchML profile and assigns the
MPU family, so future architectures can use the contract without changing the
core API. Source is copied; parser options bound allocation and work. The
builtin family restriction applies only to `init`, not custom profile names.

Code generation is host-independent and limited to supplied concrete forms.
Queries expose compiler metadata, but this module does not implement register
allocation, ABI lowering, a text assembler or an emulator. Symbolic relocation
support remains in AMD64's legacy typed API; shared emission takes resolved
numeric operands. Failures preserve emitter contents and codec outputs.

Evidence: [unit regressions](../tests/test_units.c) exercise all MPU backends,
memory and branch encoding, aliases, custom MPU profiles and lifecycle errors.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-mpu.h`](../include/mulib-mpu.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-mpu.h
 * @brief Extensible microprocessor profile, query and code emission contract.
 * Define MULIB_IMPL_MPU and UNIT's dependency closure. An MPU is an owned
 * ArchML profile; this API emits code and metadata, not host CPU instructions.
 */
#ifndef MULIB_MPU_H
#define MULIB_MPU_H
#include "mulib-unit.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_unit_t mulib_mpu_t;
/** Initialize amd64, aarch64 or riscv64. Zero initialize first; success is 1. */
mulib_archml_result mulib_mpu_init (mulib_mpu_t *, const char *, mulib_archml_error_t *);
/** Extend the contract with an owned, caller-supplied ArchML MPU profile. */
mulib_archml_result mulib_mpu_parse (mulib_mpu_t *, const char *, size_t,
    const mulib_archml_options_t *, mulib_archml_error_t *);
/** Free and reset; NULL/repeated free permitted. */
void mulib_mpu_free (mulib_mpu_t *);
/** Shared queries and transactional codecs; see mulib-unit.h for signatures.
 * Concrete form names and numeric operand order come from the loaded profile.
 * Future MPUs implement the same contract by loading their own ArchML data. */
#define mulib_mpu_document mulib_unit_document
#define mulib_mpu_find mulib_unit_find
#define mulib_mpu_register_info mulib_unit_register_info
#define mulib_mpu_enabled mulib_unit_enabled
#define mulib_mpu_encode mulib_unit_encode
#define mulib_mpu_decode mulib_unit_decode
#define mulib_mpu_emit mulib_unit_emit
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
| Header | [`include/mulib-mpu.h`](../include/mulib-mpu.h) |
| Implementation switch | `MULIB_IMPL_MPU` |
| Repository-local includes | [mulib-unit](mulib-unit.md) |
| Complete example | [`examples/modules/mpu.c`](../examples/modules/mpu.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/mpu.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
