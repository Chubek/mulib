# packetvm — bounded filter interpreter

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_PACKETVM`; no dependencies.
See [`examples/modules/packetvm.c`](../examples/modules/packetvm.c).

Programs contain up to 65536 typed instructions and end with RET. The single
32-bit accumulator starts at zero. LD8/LD16/LD32 load absolute packet offsets
in big-endian order; AND masks it; JEQ compares an immediate and chooses a
forward skip from the next instruction; RET returns immediate 0 drop or 1
accept. Unused jump fields must be zero. Both JEQ destinations must remain in
the program, even if one branch is unreachable for a particular packet.

`validate` returns 1/0 and optionally the first invalid instruction index
(SIZE_MAX if no specific instruction is responsible). `run` validates before
executing, decrements an explicit instruction budget, and bounds-checks every
packet load. Return 0 success with verdict, -1 invalid, -2 budget exhausted,
-3 packet too short. Verdict remains unchanged on error. Inputs are borrowed;
there are no allocations, packet writes, helpers, backward loops or syscalls.

This is a small MuLIB instruction set, not classic BPF, eBPF, a JIT, or a kernel
verifier. It is suitable for explicit application packet policy. Tests:
`tests/test_protocol_extensions.c` covers invalid jumps, budgets and short loads.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-packetvm.h`](../include/mulib-packetvm.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-packetvm.h
 * @brief Bounded, forward-only packet-filter bytecode interpreter.
 * Define MULIB_IMPL_PACKETVM once. This is a MuLIB VM, not kernel BPF.
 */
#ifndef MULIB_PACKETVM_H
#define MULIB_PACKETVM_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_PACKETVM_LD8 = 1,
    MULIB_PACKETVM_LD16,
    MULIB_PACKETVM_LD32,
    MULIB_PACKETVM_AND,
    MULIB_PACKETVM_JEQ,
    MULIB_PACKETVM_RET
  };
  typedef struct mulib_packetvm_instruction
  {
    unsigned opcode;
    uint32_t value;
    size_t yes, no; /* JEQ forward skips, otherwise must be zero */
  } mulib_packetvm_instruction_t;
  /** @brief Validate <=65536 instructions, final RET, forward in-range jumps.
   * RET values are 0 drop/1 accept. 1 valid, 0 invalid; bad is SIZE_MAX if
   * none. */
  int mulib_packetvm_validate (const mulib_packetvm_instruction_t *, size_t,
                               size_t *bad);
  /** @brief Evaluate from zero accumulator; big-endian absolute loads. Returns
   * 0 success with verdict, -1 invalid, -2 budget exhausted, -3 packet too
   * short. Output unchanged on error. No memory writes or external calls. */
  int mulib_packetvm_run (const mulib_packetvm_instruction_t *, size_t,
                          const void *, size_t, size_t budget, int *verdict);
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
| Header | [`include/mulib-packetvm.h`](../include/mulib-packetvm.h) |
| Implementation switch | `MULIB_IMPL_PACKETVM` |
| Repository-local includes | None |
| Complete example | [`examples/modules/packetvm.c`](../examples/modules/packetvm.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/packetvm.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
