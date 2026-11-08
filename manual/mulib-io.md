# mulib-io

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_IO` once. By default every hardware operation returns -1,
making hosted inclusion safe. For GCC/Clang x86 ring-zero code define
`MULIB_IO_ENABLE_PRIVILEGED` **before the implementation include**.
No other switches are needed. [Example](../examples/modules/io.c).

`mulib_io_in8`/`out8` perform byte port accesses. `in8` requires nonnull output.
`mulib_io_load_table` loads a 10-byte descriptor operand with LGDT (argument 0)
or LIDT (argument 1), only on x86-64. It does not reload CS, data registers or TR.
All functions return zero when executed or -1 for unavailable/invalid operations.
The port/table memory is caller-owned. There is no allocation or OS dependency.

Native instructions carry compiler memory clobbers, but do not replace device
timing, MMIO barriers or platform-specific synchronization. Opt-in is a build
gate, **not** a runtime privilege check. Enabling it in a hosted process may fault.
Interrupt state, table validity, CPU feature support and safe handoff are entirely
the caller's responsibility. Never execute the privileged example path under a
normal user-mode test runner. Other compilers/architectures return unavailable.

`mulib_io_in32`/`out32` provide actual dword port I/O under the same privileged
opt-in. They support CF8/CFC configuration access; the application must serialize
the address/data pair across CPUs and interrupts. Null input-output pointers are
rejected. Default hosted builds return -1 without touching hardware.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-io.h`](../include/mulib-io.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-io.h
 * @brief Explicit opt-in x86 port I/O and descriptor-table loads.
 * Define MULIB_IMPL_IO once and MULIB_IO_ENABLE_PRIVILEGED for ring-zero code.
 * Without opt-in operations return -1 and do not execute privileged instructions.
 */
#ifndef MULIB_IO_H
#define MULIB_IO_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Read a byte from an x86 I/O port. Returns -1 if unavailable. */
int mulib_io_in8(uint16_t port, uint8_t *value);
/** @brief Write a byte to an x86 I/O port. Returns -1 if unavailable. */
int mulib_io_out8(uint16_t port, uint8_t value);
/** @brief Read/write a 32-bit port (including PCI CF8/CFC configuration ports).
 * Concurrent configuration access requires caller serialization.
 */
int mulib_io_in32(uint16_t port, uint32_t *value);
int mulib_io_out32(uint16_t port, uint32_t value);
/** @brief Load 10-byte GDTR/IDTR operand; idt must be 0 (GDT) or 1 (IDT).
 * Does not reload segment registers, CS, or TR. Requires x86-64 ring zero.
 */
int mulib_io_load_table(const uint8_t descriptor[10], unsigned idt);
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
| Header | [`include/mulib-io.h`](../include/mulib-io.h) |
| Implementation switch | `MULIB_IMPL_IO` |
| Repository-local includes | None |
| Complete example | [`examples/modules/io.c`](../examples/modules/io.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/io.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
