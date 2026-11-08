# AVR32A

[Manuals](README.md) · [Library index](index.md)

`mulib-avr32.h` implements the [MCU contract](mulib-mcu.md) for selected
**32-bit AVR32A** forms, not the unrelated 8-bit AVR ISA. The embedded profile
contains nine concrete 16-bit instructions: add, and, or, eor, register move,
immediate move, complement, nop and breakpoint. PC register operands are excluded.
See [coverage and provenance](../library/archml/README.md).

Define `MULIB_IMPL_AVR32`, `MULIB_IMPL_MCU`, `MULIB_IMPL_UNIT`,
`MULIB_IMPL_ARCHML`, `MULIB_IMPL_PARSEC` and `MULIB_IMPL_EMITTER`. The
[example](../examples/modules/avr32.c) emits `add {2,3}` as big-endian `06 02`.

Zero initialize `mulib_avr32_t`; `init`/`init_ex` return 1/0, and `init_ex`
refreshes the optional error. Failure preserves the owner, and a live context
cannot be reinitialized. Free resets it and invalidates borrowed views;
NULL/repeated free is harmless. Live ownership must not be copied.

The type is directly usable through MCU queries and codecs. `document`, `find`,
`register_info`, `enabled`, `encode`, `decode` and `emit` are aliases of the
family contract. Operands are numeric IDs/immediates in profile order. Codecs
and emission return **MULIB_ARCHML_OK (1)** or a negative error. Emit requires
**big endian**, despite host byte order. Width and register restrictions are
validated, including rejecting `r15` where the profile excludes PC effects.
All output bytes, lengths and decoded operands remain unchanged on failure.

No peripheral, DSP/SIMD, coprocessor, general memory or privileged instruction
coverage is implied. The [unit contract](mulib-unit.md) gives allocation limits,
custom profile loading and concurrency rules. [Unit tests](../tests/test_units.c)
exercise endian enforcement, bytes, aliases, forbidden PC operands, decoding,
allocation failures and lifecycle reuse.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-avr32.h`](../include/mulib-avr32.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-avr32.h
 * @brief Owned big-endian AVR32A selected-form MCU implementation.
 * Define MULIB_IMPL_AVR32 and MCU's dependency closure. This is 32-bit AVR32A,
 * not 8-bit AVR. Profile covers selected 16-bit register/immediate forms.
 */
#ifndef MULIB_AVR32_H
#define MULIB_AVR32_H
#include "mulib-mcu.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_mcu_t mulib_avr32_t;
/** Zero initialize; return 1 on success, 0 on failure/live context. */
int mulib_avr32_init (mulib_avr32_t *);
/** Diagnostic initialization; Boolean result. */
int mulib_avr32_init_ex (mulib_avr32_t *, mulib_archml_error_t *);
/** Release/reset. NULL/repeated free permitted. */
void mulib_avr32_free (mulib_avr32_t *);
/** MCU contract aliases; numeric operands follow the profile, e.g. add {2,3}.
 * Codecs/emit return MULIB_ARCHML_OK (1) or a negative error. */
#define mulib_avr32_document mulib_mcu_document
#define mulib_avr32_find mulib_mcu_find
#define mulib_avr32_register_info mulib_mcu_register_info
#define mulib_avr32_enabled mulib_mcu_enabled
#define mulib_avr32_encode mulib_mcu_encode
#define mulib_avr32_decode mulib_mcu_decode
#define mulib_avr32_emit mulib_mcu_emit
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
| Header | [`include/mulib-avr32.h`](../include/mulib-avr32.h) |
| Implementation switch | `MULIB_IMPL_AVR32` |
| Repository-local includes | [mulib-mcu](mulib-mcu.md) |
| Complete example | [`examples/modules/avr32.c`](../examples/modules/avr32.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/avr32.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
