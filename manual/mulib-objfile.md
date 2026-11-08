# objfile

[Manuals](README.md) · [Library index](index.md)

Borrowed object model restricted to ELF ET_REL. `open` uses the existing checked
ELF32/64 parser and rejects executable/shared/other object formats. `section`
borrows bytes by exact name; NOBITS sections return ETYPE. Returns 0 success or
negative ELF errors; outputs unchanged on failure. Source owns all bytes and
must remain live/immutable while views are used.

No COFF/Mach-O, symbol linking, relocations, native execution or file I/O.
Switches: `MULIB_IMPL_OBJFILE`, `MULIB_IMPL_ELFUTILS`, `MULIB_IMPL_ELF`,
`MULIB_IMPL_ENDIAN`, `MULIB_IMPL_BINFILE`.
Example: `examples/modules/objfile.c` also generates an object using the writer.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-objfile.h`](../include/mulib-objfile.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-objfile.h
 * @brief Borrowed ELF relocatable-object model; other object formats rejected.
 * Define MULIB_IMPL_OBJFILE plus ELFUTILS, ELF and ENDIAN. */
#ifndef MULIB_OBJFILE_H
#define MULIB_OBJFILE_H
#include "mulib-binfile.h"
#include "mulib-elfutils.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_objfile
  {
    mulib_elf_t elf;
  } mulib_objfile_t;
  /** @brief Open ET_REL ELF32/64; 0 success, ELF error otherwise. No ownership
   * transfer. */
  int mulib_objfile_open (mulib_objfile_t *out, const void *data, size_t size);
  /** @brief Borrow named file-backed section; NOBITS returns ETYPE. */
  int mulib_objfile_section (const mulib_objfile_t *object, const char *name,
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
| Header | [`include/mulib-objfile.h`](../include/mulib-objfile.h) |
| Implementation switch | `MULIB_IMPL_OBJFILE` |
| Repository-local includes | [mulib-binfile](mulib-binfile.md), [mulib-elfutils](mulib-elfutils.md) |
| Complete example | [`examples/modules/objfile.c`](../examples/modules/objfile.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/objfile.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
