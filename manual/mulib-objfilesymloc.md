# objfilesymloc

[Manuals](README.md) · [Library index](index.md)

`find` resolves a defined ELF symbol's file-backed bytes. Uses section-relative
st_value for ET_REL, virtual values minus section address for EXEC/DYN. Rejects
undefined, ABS/COMMON/reserved section indexes, NOBITS and section-crossing ranges.
Returns 0 success, negative ELF error; output unchanged on failure. Borrowed
result expires with immutable source. A zero-size symbol returns an empty span.

Not runtime dynamic symbol resolution or weak/global arbitration; lookup is
the first defined exact name as in elfutils.
Switches: `MULIB_IMPL_OBJFILESYMLOC`, `MULIB_IMPL_ELFUTILS`, `MULIB_IMPL_ELF`,
`MULIB_IMPL_ENDIAN`, `MULIB_IMPL_BINFILE`. Example: `examples/modules/objfilesymloc.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-objfilesymloc.h`](../include/mulib-objfilesymloc.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-objfilesymloc.h
 * @brief Resolve a defined ELF symbol's file-backed bytes, not its runtime
 * address. Define MULIB_IMPL_OBJFILESYMLOC plus ELFUTILS, ELF and ENDIAN. */
#ifndef MULIB_OBJFILESYMLOC_H
#define MULIB_OBJFILESYMLOC_H
#include "mulib-binfile.h"
#include "mulib-elfutils.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief ET_REL st_value is section-relative; EXEC/DYN values are virtual.
   * Reject ABS/COMMON/undefined/NOBITS and section-crossing symbols.
   * 0 success, ELF error otherwise. Output borrows input. */
  int mulib_objfilesymloc_find (const mulib_elf_t *elf, const char *name,
                                mulib_binfile_t *out);
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
| Header | [`include/mulib-objfilesymloc.h`](../include/mulib-objfilesymloc.h) |
| Implementation switch | `MULIB_IMPL_OBJFILESYMLOC` |
| Repository-local includes | [mulib-binfile](mulib-binfile.md), [mulib-elfutils](mulib-elfutils.md) |
| Complete example | [`examples/modules/objfilesymloc.c`](../examples/modules/objfilesymloc.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/objfilesymloc.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
