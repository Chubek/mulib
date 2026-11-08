# AArch64

[Manuals](README.md) · [Library index](index.md)

`mulib-aarch64.h` implements the [MPU contract](mulib-mpu.md) using the embedded
`arm64` ArchML profile. It exposes selected 64/32-bit integer, load/store,
branch, floating-point and vector forms plus register, feature and ABI metadata.
See the authoritative [profile coverage](../library/archml/README.md).

Define `MULIB_IMPL_AARCH64`, `MULIB_IMPL_MPU`, `MULIB_IMPL_UNIT`,
`MULIB_IMPL_ARCHML`, `MULIB_IMPL_PARSEC` and `MULIB_IMPL_EMITTER`. The complete
[example](../examples/modules/aarch64.c) emits `add64 {0,1,2}` as `20 00 02 8b`.

Zero initialize `mulib_aarch64_t`, call `init` or `init_ex`, and free with
`mulib_aarch64_free`. Both initializers return Boolean 1/0; `init_ex` refreshes
an optional ArchML diagnostic. Failure preserves the context; reinitializing a
live context fails. NULL/repeated free is harmless. Metadata is owned, query
results borrowed until free, and live contexts must not be copied.

The type is directly usable as `mulib_mpu_t`. `document`, `find`,
`register_info`, `enabled`, `encode`, `decode` and `emit` alias the MPU API.
Codecs and emission return **MULIB_ARCHML_OK (1)** or a negative error. Operands
are numeric encoding IDs and immediate bit patterns in concrete-form order;
`add64` is a form name, not parsed assembly. Branch displacements are bytes.
Binary emit requires a little-endian emitter. Outputs remain unchanged on
failure. Named decoding consumes a prefix and requires the expected form.

Aliases such as `w0`/`x0` expose overlapping storage; `sp`/`xzr` require the
selected form's register class semantics. Feature availability must be queried
explicitly. Encoders do not probe host capabilities or execute instructions.
The [unit manual](mulib-unit.md) specifies bounds, concurrency and custom-profile
extension. [Regressions](../tests/test_units.c) cover bytes, aliases, decoding,
short buffers and lifecycle failures.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-aarch64.h`](../include/mulib-aarch64.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-aarch64.h
 * @brief Owned AArch64 profile implementing the MPU contract.
 * Define MULIB_IMPL_AARCH64 and MPU's dependency closure. Concrete forms and
 * aliases come from the embedded arm64 ArchML profile; all hosts supported.
 */
#ifndef MULIB_AARCH64_H
#define MULIB_AARCH64_H
#include "mulib-mpu.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_mpu_t mulib_aarch64_t;
/** Zero initialize first. Return 1 on success, 0 on failure/live context. */
int mulib_aarch64_init (mulib_aarch64_t *);
/** Diagnostic variant with the same Boolean return convention. */
int mulib_aarch64_init_ex (mulib_aarch64_t *, mulib_archml_error_t *);
/** Release/reset owned profile; NULL/repeated free permitted. */
void mulib_aarch64_free (mulib_aarch64_t *);
/** MPU contract aliases: codecs/emit return MULIB_ARCHML_OK (1), not Boolean.
 * Operands are numeric profile values, e.g. add64 {0,1,2}; branches use byte
 * displacements. See mulib-mpu.h and mulib-unit.h for full signatures. */
#define mulib_aarch64_document mulib_mpu_document
#define mulib_aarch64_find mulib_mpu_find
#define mulib_aarch64_register_info mulib_mpu_register_info
#define mulib_aarch64_enabled mulib_mpu_enabled
#define mulib_aarch64_encode mulib_mpu_encode
#define mulib_aarch64_decode mulib_mpu_decode
#define mulib_aarch64_emit mulib_mpu_emit
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
| Header | [`include/mulib-aarch64.h`](../include/mulib-aarch64.h) |
| Implementation switch | `MULIB_IMPL_AARCH64` |
| Repository-local includes | [mulib-mpu](mulib-mpu.md) |
| Complete example | [`examples/modules/aarch64.c`](../examples/modules/aarch64.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/aarch64.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
