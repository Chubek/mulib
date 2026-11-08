# Xtensa

[Manuals](README.md) · [Library index](index.md)

`mulib-xtensa.h` implements the [MCU contract](mulib-mcu.md) using a selected
little-endian Xtensa core integer profile. It supplies twenty concrete 24-bit
arithmetic, logical, immediate and load/store forms with visible `a0..a15`
registers. Window rotation, density, configurable extensions, FP, branches and
calling-convention lowering are outside this profile. See
[coverage](../library/archml/README.md).

Define `MULIB_IMPL_XTENSA`, `MULIB_IMPL_MCU`, `MULIB_IMPL_UNIT`,
`MULIB_IMPL_ARCHML`, `MULIB_IMPL_PARSEC` and `MULIB_IMPL_EMITTER` once.
The [example](../examples/modules/xtensa.c) emits `add {2,3,4}` as `40 23 80`.

Zero initialize the context; `init`/`init_ex` return Boolean 1/0. `init_ex`
refreshes an optional diagnostic. Failure preserves the owner, live contexts
are rejected, and free resets the context for reuse. NULL/repeated free is
harmless. Borrowed profile views expire at free; do not copy live ownership.

The type and query/codec aliases directly implement the MCU API. Concrete-form
operands are numeric register IDs and immediate bit patterns. Codecs and emit
return **MULIB_ARCHML_OK (1)** or a negative error. Instruction length is three
bytes, independent of the 32-bit register width. Binary emit requires little
endian. Scaled load/store immediates are byte offsets and must satisfy the
form's alignment/range. Decoding requires the expected form and consumes one
prefix. Every failure preserves output bytes, lengths and operands.

Features and legality beyond encoding are checked separately. The
[unit contract](mulib-unit.md) specifies limits, ownership, concurrency and
custom profile extension. [Regressions](../tests/test_units.c) verify known
bytes, aliases, transactional decoding/emission and lifecycle errors.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-xtensa.h`](../include/mulib-xtensa.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-xtensa.h
 * @brief Owned little-endian Xtensa core 24-bit MCU implementation.
 * Define MULIB_IMPL_XTENSA and MCU's dependency closure. Only the embedded
 * profile's integer forms and visible a0..a15 registers are supported.
 */
#ifndef MULIB_XTENSA_H
#define MULIB_XTENSA_H
#include "mulib-mcu.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_mcu_t mulib_xtensa_t;
/** Zero initialize; return 1 on success, 0 on failure/live context. */
int mulib_xtensa_init (mulib_xtensa_t *);
/** Diagnostic initialization; Boolean result. */
int mulib_xtensa_init_ex (mulib_xtensa_t *, mulib_archml_error_t *);
/** Release/reset. NULL/repeated free permitted. */
void mulib_xtensa_free (mulib_xtensa_t *);
/** MCU contract aliases; numeric operands follow the profile.
 * Codecs/emit return MULIB_ARCHML_OK (1) or a negative error. */
#define mulib_xtensa_document mulib_mcu_document
#define mulib_xtensa_find mulib_mcu_find
#define mulib_xtensa_register_info mulib_mcu_register_info
#define mulib_xtensa_enabled mulib_mcu_enabled
#define mulib_xtensa_encode mulib_mcu_encode
#define mulib_xtensa_decode mulib_mcu_decode
#define mulib_xtensa_emit mulib_mcu_emit
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
| Header | [`include/mulib-xtensa.h`](../include/mulib-xtensa.h) |
| Implementation switch | `MULIB_IMPL_XTENSA` |
| Repository-local includes | [mulib-mcu](mulib-mcu.md) |
| Complete example | [`examples/modules/xtensa.c`](../examples/modules/xtensa.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/xtensa.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
