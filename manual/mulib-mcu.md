# MCU — microcontroller contract

[Manuals](README.md) · [Library index](index.md)

`mulib-mcu.h` is the architecture/code-generation contract for
[AVR32A](mulib-avr32.md), [Xtensa](mulib-xtensa.md) and future MCU profiles.
It exposes explicit target width and endian order, owned register/instruction
metadata, feature queries and transactional binary encoding/decoding.

## Integration and lifecycle

Define `MULIB_IMPL_MCU`, `MULIB_IMPL_UNIT`, `MULIB_IMPL_ARCHML`,
`MULIB_IMPL_PARSEC` and `MULIB_IMPL_EMITTER` in one implementation translation
unit. The [example](../examples/modules/mcu.c) emits an AVR32A instruction
without external files or target hardware.

Zero initialize `mulib_mcu_t`, call `mulib_mcu_init` with `avr32` or `xtensa`,
then free with `mulib_mcu_free`. Success is `MULIB_ARCHML_OK` (1); failures are
negative, refresh the optional diagnostic and preserve the context. Live
contexts cannot be reinitialized. Free is NULL-safe, repeatable and permits
reuse. Documents are owned; metadata views are borrowed until free. Never copy
a live context or modify fields.

## Code-generation behavior

The query and codec aliases use the [shared unit contract](mulib-unit.md).
`register_info` exposes architectural aliases and encoding IDs. `encode` takes
numeric operands in profile order; `decode` takes a named form and consumes one
prefix. `emit` appends atomically, checking capacity and target byte order.
AVR32A uses big endian; the supplied Xtensa profile uses little endian. Instruction
lengths come from the profile, not the 32-bit register width. Invalid operands,
unsupported forms, short buffers and endian mismatches preserve outputs.

`mulib_mcu_parse` copies a caller-supplied ArchML profile and assigns the MCU
family. This is the extension path for future cores, including explicit memory,
layout and instruction metadata. ArchML parse options and the unit's 1..64-bit
target constraint apply. Encoders allocate no memory; profile initialization
allocates owned data. Read-only access can be concurrent, while free requires
external synchronization.

These modules generate selected instruction forms. Device registers, MMIO,
interrupt delivery, reset code, peripheral configuration, flash programming and
board-specific behavior require separate implementation. The core name alone
does not select a particular chip's configurable extensions.

Evidence: [unit regressions](../tests/test_units.c) cover both byte orders,
16/24-bit instruction lengths, register restrictions, custom MCU profiles and
allocation failure recovery.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-mcu.h`](../include/mulib-mcu.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-mcu.h
 * @brief Extensible microcontroller architecture and encoding contract.
 * Define MULIB_IMPL_MCU and UNIT's dependency closure. Device peripherals,
 * flash programming, interrupts and board initialization are outside this API.
 */
#ifndef MULIB_MCU_H
#define MULIB_MCU_H
#include "mulib-unit.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_unit_t mulib_mcu_t;
/** Initialize avr32 or xtensa. Zero initialize first; success is 1. */
mulib_archml_result mulib_mcu_init (mulib_mcu_t *, const char *, mulib_archml_error_t *);
/** Load a future/custom MCU profile with explicit endian and register data. */
mulib_archml_result mulib_mcu_parse (mulib_mcu_t *, const char *, size_t,
    const mulib_archml_options_t *, mulib_archml_error_t *);
/** Release all owned profile data and reset. NULL/repeated free permitted. */
void mulib_mcu_free (mulib_mcu_t *);
/** Shared queries/codecs inherit mulib-unit.h ownership and result contracts. */
#define mulib_mcu_document mulib_unit_document
#define mulib_mcu_find mulib_unit_find
#define mulib_mcu_register_info mulib_unit_register_info
#define mulib_mcu_enabled mulib_unit_enabled
#define mulib_mcu_encode mulib_unit_encode
#define mulib_mcu_decode mulib_unit_decode
#define mulib_mcu_emit mulib_unit_emit
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
| Header | [`include/mulib-mcu.h`](../include/mulib-mcu.h) |
| Implementation switch | `MULIB_IMPL_MCU` |
| Repository-local includes | [mulib-unit](mulib-unit.md) |
| Complete example | [`examples/modules/mcu.c`](../examples/modules/mcu.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/mcu.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
