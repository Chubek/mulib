# objfileleader

[Manuals](README.md) · [Library index](index.md)

Historical filename for an ELF PT_LOAD copy/zero-fill helper, **not execution**.
`load` accepts EXEC/DYN views, a destination virtual base and caller memory size.
It prevalidates every load segment's bounds and rejects overlapping memory
segments before copying. File-backed bytes are copied, BSS zeroed, gaps untouched.
Source bytes and destination must not overlap. Returns 0 success, ELF error otherwise.

No memory mapping, permissions, interpreter, dynamic linking, relocation or
entry-point execution. Caller must retain validated immutable ELF input.
Switches: `MULIB_IMPL_OBJFILELEADER`, `MULIB_IMPL_ELF`, `MULIB_IMPL_ENDIAN`.
Example: `examples/modules/objfileleader.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-objfileleader.h`](../include/mulib-objfileleader.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-objfileleader.h
 * @brief ELF PT_LOAD copy/zero-fill helper (historical filename: leader).
 * Define MULIB_IMPL_OBJFILELEADER plus ELF/ENDIAN. No mapping, relocation or
 * execution. */
#ifndef MULIB_OBJFILELEADER_H
#define MULIB_OBJFILELEADER_H
#include "mulib-elf.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Copy EXEC/DYN load segments into caller memory representing
   * [base,base+size). Prevalidate all bounds and reject overlapping segments.
   * Input must not alias destination. Gaps untouched, BSS zeroed. 0 success,
   * ELF error otherwise. Permissions, interpreter, dynamic linking and
   * entry-point execution are caller-owned. */
  int mulib_objfileleader_load (const mulib_elf_t *elf, uint64_t base,
                                void *memory, size_t size);
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
| Header | [`include/mulib-objfileleader.h`](../include/mulib-objfileleader.h) |
| Implementation switch | `MULIB_IMPL_OBJFILELEADER` |
| Repository-local includes | [mulib-elf](mulib-elf.md) |
| Complete example | [`examples/modules/objfileleader.c`](../examples/modules/objfileleader.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/objfileleader.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
