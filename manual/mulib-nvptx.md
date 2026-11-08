# NVPTX64

[Manuals](README.md) · [Library index](index.md)

`mulib-nvptx.h` implements the text-operation side of the
[GPU contract](mulib-gpu.md). The embedded profile supplies fifteen selected
unpredicated integer arithmetic/bitwise operations over typed virtual values.
It emits **PTX text**, not NVIDIA SASS machine code. See
[profile coverage](../library/archml/README.md).

Define `MULIB_IMPL_NVPTX`, `MULIB_IMPL_GPU`, `MULIB_IMPL_UNIT`,
`MULIB_IMPL_ARCHML`, `MULIB_IMPL_PARSEC` and `MULIB_IMPL_EMITTER`. The
[example](../examples/modules/nvptx.c) emits:

```ptx
add.s32 %r0, %r1, %r2;
```

Zero initialize `mulib_nvptx_t`. `init`/`init_ex` return 1/0; the latter refreshes
an optional ArchML error. Failed initialization preserves the context and
reinitializing a live context fails. Free resets ownership; NULL/repeated free
is harmless. Borrowed metadata expires at free. Do not copy live ownership.

`document`, `find` and `enabled` alias GPU queries. There is no finite physical
register bank in the profile. `encode`/`decode` explicitly return MISMATCH for
text operations and leave outputs unchanged. `mulib_nvptx_emit` aliases
`mulib_gpu_emit_text`, taking an operation name and an array of virtual-register
strings in profile order. It returns **MULIB_ARCHML_OK (1)** or a negative error.

Register strings must match `%[A-Za-z_$][A-Za-z0-9_$]*` and be at most 128 bytes.
Immediate expressions are not accepted. Output includes a trailing newline,
but no NUL; use the emitter size to consume it. Either valid emitter byte order
works. The renderer uses at most 4096 bytes of scratch space and 16 operands,
does not allocate and leaves output untouched on any error. See the
[GPU manual](mulib-gpu.md) for diagnostics and template validation.

The caller supplies matching register types/declarations, PTX version and SM
target, module/kernel framing and ABI. Predication, memory spaces, kernel
launches and PTX-to-SASS compilation are separate concerns. Read-only contexts
can be shared concurrently; emitter mutation and free need caller coordination.
[Tests](../tests/test_units.c) exercise all supplied operations, injection-like
invalid identifiers, operand count errors, short buffers and binary rejection.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-nvptx.h`](../include/mulib-nvptx.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-nvptx.h
 * @brief Owned NVPTX64 virtual-ISA text GPU implementation.
 * Define MULIB_IMPL_NVPTX and GPU's dependency closure. Selected unpredicated
 * integer operations emit PTX text; this API never invents NVIDIA SASS bytes.
 */
#ifndef MULIB_NVPTX_H
#define MULIB_NVPTX_H
#include "mulib-gpu.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_gpu_t mulib_nvptx_t;
/** Zero initialize; return 1 on success, 0 on failure/live context. */
int mulib_nvptx_init (mulib_nvptx_t *);
/** Diagnostic initialization; Boolean result. */
int mulib_nvptx_init_ex (mulib_nvptx_t *, mulib_archml_error_t *);
/** Release/reset. NULL/repeated free permitted. */
void mulib_nvptx_free (mulib_nvptx_t *);
/** GPU queries and explicit binary rejection. Text operands are virtual
 * register names; mulib_nvptx_emit appends an operation plus newline.
 * Emission returns MULIB_ARCHML_OK (1) or a negative error. */
#define mulib_nvptx_document mulib_gpu_document
#define mulib_nvptx_find mulib_gpu_find
#define mulib_nvptx_enabled mulib_gpu_enabled
#define mulib_nvptx_encode mulib_gpu_encode
#define mulib_nvptx_decode mulib_gpu_decode
#define mulib_nvptx_emit mulib_gpu_emit_text
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
| Header | [`include/mulib-nvptx.h`](../include/mulib-nvptx.h) |
| Implementation switch | `MULIB_IMPL_NVPTX` |
| Repository-local includes | [mulib-gpu](mulib-gpu.md) |
| Complete example | [`examples/modules/nvptx.c`](../examples/modules/nvptx.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/nvptx.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
