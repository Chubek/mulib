# RISC-V64

[Manuals](README.md) · [Library index](index.md)

`mulib-riscv64.h` implements the [MPU contract](mulib-mpu.md) with the embedded
RV64 ArchML profile. Selected integer, atomic, double-precision floating-point
and compressed forms coexist with metadata-only extensions; inspect the
[profile coverage](../library/archml/README.md) before selecting a form.

Define `MULIB_IMPL_RISCV64`, `MULIB_IMPL_MPU`, `MULIB_IMPL_UNIT`,
`MULIB_IMPL_ARCHML`, `MULIB_IMPL_PARSEC` and `MULIB_IMPL_EMITTER`. The
[example](../examples/modules/riscv64.c) encodes and decodes
`add {10,11,12}` (`a0,a1,a2`) as `33 85 c5 00` on any host.

Zero initialize the context. `init`/`init_ex` return Boolean 1/0; `init_ex`
also refreshes an optional ArchML error. Live-context initialization is rejected,
and failure leaves the context unchanged. Free resets ownership and permits
reuse; NULL/repeated free is safe. Borrowed views expire at free. Do not copy a
live owner.

`mulib_riscv64_t` is usable as `mulib_mpu_t`. Query/codec aliases expose the
same API and limits as the [shared unit contract](mulib-unit.md). Codecs and
emission return **MULIB_ARCHML_OK (1)** or negative errors. Register operands
are encoding IDs; ABI names such as `a0` can be resolved with `register_info`.
Signed immediates are uint64 two's-complement patterns. Branch and jump operands
are **byte displacements**, including for scattered fields; unaligned and
out-of-range values are rejected. Binary emit requires little endian and
preserves storage and size on error. No label resolution or relaxation occurs.

Call `enabled` with an explicit feature set before selecting extension forms.
`decode` is named-form decoding, not instruction discovery. Metadata does not
enable OS vector/FP state or establish ABI compatibility. The
[tests](../tests/test_units.c) exercise known bytes, aliases, JAL signed range
boundaries, misalignment, decoding and failed emission rollback.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-riscv64.h`](../include/mulib-riscv64.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-riscv64.h
 * @brief Owned RV64 profile implementing the MPU contract.
 * Define MULIB_IMPL_RISCV64 and MPU's dependency closure. Encodings and
 * register aliases come from the embedded riscv64 ArchML profile.
 */
#ifndef MULIB_RISCV64_H
#define MULIB_RISCV64_H
#include "mulib-mpu.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_mpu_t mulib_riscv64_t;
/** Zero initialize first. Return 1 on success, 0 on failure/live context. */
int mulib_riscv64_init (mulib_riscv64_t *);
/** Diagnostic variant with the same Boolean return convention. */
int mulib_riscv64_init_ex (mulib_riscv64_t *, mulib_archml_error_t *);
/** Release/reset owned profile; NULL/repeated free permitted. */
void mulib_riscv64_free (mulib_riscv64_t *);
/** MPU contract aliases. Encoders consume numeric register IDs/immediates,
 * e.g. add {10,11,12}; signed and scattered branch operands are byte offsets.
 * Codecs/emit return MULIB_ARCHML_OK (1) or a negative error. */
#define mulib_riscv64_document mulib_mpu_document
#define mulib_riscv64_find mulib_mpu_find
#define mulib_riscv64_register_info mulib_mpu_register_info
#define mulib_riscv64_enabled mulib_mpu_enabled
#define mulib_riscv64_encode mulib_mpu_encode
#define mulib_riscv64_decode mulib_mpu_decode
#define mulib_riscv64_emit mulib_mpu_emit
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
| Header | [`include/mulib-riscv64.h`](../include/mulib-riscv64.h) |
| Implementation switch | `MULIB_IMPL_RISCV64` |
| Repository-local includes | [mulib-mpu](mulib-mpu.md) |
| Complete example | [`examples/modules/riscv64.c`](../examples/modules/riscv64.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/riscv64.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
