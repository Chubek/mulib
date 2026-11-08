# ebpf

[Manuals](README.md) · [Library index](index.md)

Forward-jump-only register eBPF subset. Instructions: MOV64 K/X (b7/bf), ADD64
K/X (07/0f), JEQ K (15), JA (05), EXIT (95). r0..r9 initially zero, signed
immediates sign-extend; addition wraps uint64. `validate` enforces canonical unused
fields, bounded forward targets, <=65536 instructions and a final EXIT. It returns
1 valid, 0 invalid with optional bad index (SIZE_MAX for non-instruction errors).

`run` returns 0 with r0, -1 invalid, -2 exhausted step budget; output unchanged on
error. Earlier EXIT is allowed. No memory, helpers, packet input, maps, syscalls,
kernel loading or JIT. This is **not Linux's verifier or a security sandbox**.
Public instruction structs are decoded values, not native/wire eight-byte layouts.
Switch: `MULIB_IMPL_EBPF`. Example: `examples/modules/ebpf.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ebpf.h`](../include/mulib-ebpf.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-ebpf.h
 * @brief Bounded register-only eBPF instruction subset interpreter, not kernel
 * BPF. Define MULIB_IMPL_EBPF once. No syscalls, helpers, packet memory, JIT
 * or kernel loading. */
#ifndef MULIB_EBPF_H
#define MULIB_EBPF_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_ebpf_instruction
  {
    unsigned char opcode, dst, src;
    int16_t offset;
    int32_t immediate;
  } mulib_ebpf_instruction_t;
  /** @brief Validate MOV64 K/X (b7/bf), ADD64 K/X (07/0f), JEQ K (15),
   * forward JA (05), EXIT (95). r0..r9 only, all initially zero. Forward-only
   * jumps, <=65536 instructions, final EXIT required; unused fields must be
   * zero. 1 valid, 0 invalid. This is not the Linux verifier or a sandbox
   * guarantee. */
  int mulib_ebpf_validate (const mulib_ebpf_instruction_t *code, size_t count,
                           size_t *bad_instruction);
  /** @brief Execute validated program, at most max_steps. 0 success, -1
   * invalid, -2 budget exhausted. Returns r0, unchanged on error; ALU uses
   * modulo-2^64. Signed immediates are sign-extended, as in eBPF ALU64. */
  int mulib_ebpf_run (const mulib_ebpf_instruction_t *code, size_t count,
                      size_t max_steps, uint64_t *out);
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
| Header | [`include/mulib-ebpf.h`](../include/mulib-ebpf.h) |
| Implementation switch | `MULIB_IMPL_EBPF` |
| Repository-local includes | None |
| Complete example | [`examples/modules/ebpf.c`](../examples/modules/ebpf.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ebpf.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
