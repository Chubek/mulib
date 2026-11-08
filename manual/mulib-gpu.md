# GPU — binary and virtual-ISA contract

[Manuals](README.md) · [Library index](index.md)

`mulib-gpu.h` supplies owned GPU architecture profiles and two explicit output
paths: concrete machine-code forms and non-binary operation templates. The
derived implementations are [AMDGPU gfx600](mulib-amdgpu.md) and
[NVPTX64](mulib-nvptx.md).

## Integration and lifecycle

Define `MULIB_IMPL_GPU`, `MULIB_IMPL_UNIT`, `MULIB_IMPL_ARCHML`,
`MULIB_IMPL_PARSEC` and `MULIB_IMPL_EMITTER` once. No GPU driver, SDK, device or
runtime asset is needed. See the [example](../examples/modules/gpu.c).

Zero initialize `mulib_gpu_t` and call `mulib_gpu_init` with `amdgpu` or `nvptx`.
Success is `MULIB_ARCHML_OK` (1), failures are negative. Failed initialization
leaves the context unchanged; live contexts are rejected. `free` resets the
owner and invalidates borrowed metadata; NULL/repeated free is harmless. Do not
copy live ownership. `parse` copies an arbitrary custom ArchML GPU profile,
subject to its options and the [shared unit limits](mulib-unit.md).

## Binary versus text output

`document`, `find`, `register_info`, `enabled`, `encode`, `decode` and `emit`
are aliases of the shared unit functions. Binary codecs accept only concrete
`instruction` records. A text-only `operation` produces MISMATCH and leaves
outputs untouched. Binary emit enforces target endian order. Feature, mode and
ABI validation are separate consumer responsibilities.

`mulib_gpu_emit_text` looks up an `operation` with `(emission text)` and an
assembly template. Operands are supplied in the profile's declared order;
`{name}` placeholders are replaced with the corresponding string. The current
text profile supports `kind value` operands supplied as virtual-register names:

```text
%[A-Za-z_$][A-Za-z0-9_$]*
```

Names must be NUL-terminated, nonempty and at most 128 bytes, including `%`.
Expressions, immediate constants and injected assembly fragments are rejected.
The renderer checks operand count and placeholder resolution; the caller checks
register declarations and types. It appends one newline and **no NUL byte**.
Both valid emitter byte orders are accepted for text. Output is limited to
4096 bytes including newline and 16 operands; rendering allocates no memory.
All error paths preserve the emitter's bytes and size. Argument errors are
INVALID, unknown/non-text operations MISMATCH, malformed templates SCHEMA and
capacity/rendering limits LIMIT. The optional diagnostic is refreshed, with a
NULL message on success. Operand strings may refer to emitter storage because
rendering completes before the append.

For NVPTX, supply PTX version/SM directives, virtual-register declarations,
kernel ABI and module framing separately. This API does not compile PTX to SASS.
For AMDGPU, supply the target-specific execution environment separately. Neither
path launches kernels, allocates device memory or discovers hardware.

Evidence: [unit tests](../tests/test_units.c) check gfx600 byte fixtures, rejected
special-register encodings, every supplied PTX operation, binary rejection for
PTX, malformed identifiers and transactional short-buffer failures.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-gpu.h`](../include/mulib-gpu.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-gpu.h
 * @brief GPU binary-profile and virtual-ISA text emission contract.
 * Define MULIB_IMPL_GPU and UNIT's dependency closure. No device runtime,
 * kernel launch or hardware discovery is performed.
 */
#ifndef MULIB_GPU_H
#define MULIB_GPU_H
#include "mulib-unit.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_unit_t mulib_gpu_t;
/** Initialize amdgpu (gfx600) or nvptx (virtual ISA). Success is 1. */
mulib_archml_result mulib_gpu_init (mulib_gpu_t *, const char *, mulib_archml_error_t *);
/** Load an owned future/custom GPU profile. Zero initialize first. */
mulib_archml_result mulib_gpu_parse (mulib_gpu_t *, const char *, size_t,
    const mulib_archml_options_t *, mulib_archml_error_t *);
/** Release and reset. NULL/repeated free permitted. */
void mulib_gpu_free (mulib_gpu_t *);
/** Shared queries/codecs. Text-only operations return MISMATCH from codecs. */
#define mulib_gpu_document mulib_unit_document
#define mulib_gpu_find mulib_unit_find
#define mulib_gpu_register_info mulib_unit_register_info
#define mulib_gpu_enabled mulib_unit_enabled
#define mulib_gpu_encode mulib_unit_encode
#define mulib_gpu_decode mulib_unit_decode
#define mulib_gpu_emit mulib_unit_emit
/** Render an operation's assembly template, substituting named operands in
 * profile order. Only virtual-register value operands are supported: names
 * match %[A-Za-z_$][A-Za-z0-9_$]*, at most 128 bytes. No immediate expressions.
 * Appends a newline, no NUL; emitter order is irrelevant. Max rendered size is
 * 4096 bytes, max operands MULIB_ARCHML_MAX_OPERANDS. Failure is transactional.
 * Caller supplies declarations/module directives and checks operand types. */
mulib_archml_result mulib_gpu_emit_text (const mulib_gpu_t *, mulib_emitter_t *,
    const char *operation, const char *const *operands, size_t,
    mulib_archml_error_t *);
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
| Header | [`include/mulib-gpu.h`](../include/mulib-gpu.h) |
| Implementation switch | `MULIB_IMPL_GPU` |
| Repository-local includes | [mulib-unit](mulib-unit.md) |
| Complete example | [`examples/modules/gpu.c`](../examples/modules/gpu.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/gpu.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
