# elfutils

[Manuals](README.md) · [Library index](index.md)

Named lookups over a validated borrowed ELF32/64 view. `section` returns the
first exact named section plus optional index. `symbol` searches SYMTAB/DYNSYM
for the first defined named symbol, including local symbols. Outputs change only
on success. Returns existing ELF result codes: 0 success, ENOTFOUND missing,
other negative structural/bounds/type/invalid errors.

No dynamic loader, cross-object resolution, weak/strong symbol arbitration or
runtime address mapping. Input must remain immutable/live after `elf_open`.
Switches: `MULIB_IMPL_ELFUTILS`, `MULIB_IMPL_ELF`, `MULIB_IMPL_ENDIAN`.
Example: `examples/modules/elfutils.c` also uses the object writer to build input.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-elfutils.h`](../include/mulib-elfutils.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-elfutils.h
 * @brief Named ELF section/symbol lookup over the checked ELF reader.
 * Define MULIB_IMPL_ELFUTILS plus ELF and ENDIAN. */
#ifndef MULIB_ELFUTILS_H
#define MULIB_ELFUTILS_H
#include "mulib-elf.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief First exact section name; 0 success, ENOTFOUND or ELF error. */
  int mulib_elfutils_section (const mulib_elf_t *elf, const char *name,
                              size_t *index, mulib_elf_section_t *out);
  /** @brief First defined named symbol in SYMTAB/DYNSYM, including local
   * symbols. 0 success, ENOTFOUND or structural error; output unchanged on
   * failure. */
  int mulib_elfutils_symbol (const mulib_elf_t *elf, const char *name,
                             mulib_elf_symbol_t *out);
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
| Header | [`include/mulib-elfutils.h`](../include/mulib-elfutils.h) |
| Implementation switch | `MULIB_IMPL_ELFUTILS` |
| Repository-local includes | [mulib-elf](mulib-elf.md) |
| Complete example | [`examples/modules/elfutils.c`](../examples/modules/elfutils.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/elfutils.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
