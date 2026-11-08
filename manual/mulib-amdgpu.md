# AMDGPU gfx600

[Manuals](README.md) · [Library index](index.md)

`mulib-amdgpu.h` implements the [GPU contract](mulib-gpu.md) for the embedded
**GCN gfx600 wave64 SALU subset**. Eight selected scalar arithmetic/logical,
move and control forms are concrete machine encodings. Operands use ordinary
`s0..s103` registers; inline constants, literal words and special-register
encodings are excluded. This is not a cross-generation AMDGPU assembler; VALU,
memory, HSA ABI, R600 and RDNA are outside the profile. See
[coverage](../library/archml/README.md).

Define `MULIB_IMPL_AMDGPU`, `MULIB_IMPL_GPU`, `MULIB_IMPL_UNIT`,
`MULIB_IMPL_ARCHML`, `MULIB_IMPL_PARSEC` and `MULIB_IMPL_EMITTER` once.
The [example](../examples/modules/amdgpu.c) emits
`s_add_u32 {0,1,2}` as `01 02 00 80` without a device or driver.

Zero initialize; `init`/`init_ex` return 1/0. The diagnostic variant refreshes
an optional ArchML error. Failed initialization preserves the context, and live
contexts cannot be reinitialized. Free resets ownership and invalidates
borrowed views; NULL/repeated free is safe. Never copy a live owner.

The type is directly usable as `mulib_gpu_t`; queries/codecs alias the family
API. Numeric operands follow the concrete profile form, not parsed assembly.
Codecs and emission return **MULIB_ARCHML_OK (1)** or a negative error. Binary
emit requires little endian. Failed encoding, decoding or emission preserves
all outputs. `decode` validates the named form and consumes a prefix.

The [unit contract](mulib-unit.md) defines resource limits, explicit feature
checks, custom profile support and synchronization. This module does not launch
waves, allocate device memory or package kernels. [Tests](../tests/test_units.c)
check golden bytes, SGPR aliases/ranges and transactional failures.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-amdgpu.h`](../include/mulib-amdgpu.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-amdgpu.h
 * @brief Owned AMDGPU gfx600 wave64 SALU GPU implementation.
 * Define MULIB_IMPL_AMDGPU and GPU's dependency closure. Encodings are the
 * embedded profile's selected scalar forms, not portable across generations.
 */
#ifndef MULIB_AMDGPU_H
#define MULIB_AMDGPU_H
#include "mulib-gpu.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_gpu_t mulib_amdgpu_t;
/** Zero initialize; return 1 on success, 0 on failure/live context. */
int mulib_amdgpu_init (mulib_amdgpu_t *);
/** Diagnostic initialization; Boolean result. */
int mulib_amdgpu_init_ex (mulib_amdgpu_t *, mulib_archml_error_t *);
/** Release/reset. NULL/repeated free permitted. */
void mulib_amdgpu_free (mulib_amdgpu_t *);
/** GPU contract aliases; register operands are ordinary s0..s103 IDs.
 * Codecs/emit return MULIB_ARCHML_OK (1) or a negative error. */
#define mulib_amdgpu_document mulib_gpu_document
#define mulib_amdgpu_find mulib_gpu_find
#define mulib_amdgpu_register_info mulib_gpu_register_info
#define mulib_amdgpu_enabled mulib_gpu_enabled
#define mulib_amdgpu_encode mulib_gpu_encode
#define mulib_amdgpu_decode mulib_gpu_decode
#define mulib_amdgpu_emit mulib_gpu_emit
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
| Header | [`include/mulib-amdgpu.h`](../include/mulib-amdgpu.h) |
| Implementation switch | `MULIB_IMPL_AMDGPU` |
| Repository-local includes | [mulib-gpu](mulib-gpu.md) |
| Complete example | [`examples/modules/amdgpu.c`](../examples/modules/amdgpu.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/amdgpu.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
