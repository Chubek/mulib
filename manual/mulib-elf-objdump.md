# ELF dump adapter

[Manuals](README.md) · [Library index](index.md)

Include `mulib-elf-objdump.h`. Define `MULIB_IMPL_ELF_OBJDUMP`, `MULIB_IMPL_ELF`,
`MULIB_IMPL_ENDIAN`, `MULIB_IMPL_OBJDUMP`, `MULIB_IMPL_TERMSTYLE`,
`MULIB_IMPL_TERMINFO`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_CSS`, `MULIB_IMPL_XML`,
`MULIB_IMPL_UTF8`, `MULIB_IMPL_VTERM`, `MULIB_IMPL_GRID` before inclusion in one TU.
[Example](../examples/modules/elf-objdump.c).

`mulib_elf_objdump(data, length, options, out)` validates ELF32/ELF64 in either
byte order and appends a complete dump atomically. Returns 1 on success and 0
on malformed bytes, requested record limit, unavailable style or allocation error.
The output is an initialized owned string buffer; no input ownership is taken.

HEADERS prints file class/machine/type and program records; SECTIONS prints
section metadata; SYMBOLS prints all SYMTAB/DYNSYM records, including undefined
and local symbols. CONTENTS adds bounded hex dumps of file-backed non-null,
non-NOBITS sections. Missing section-name tables produce empty names. Invalid
present names or symbols are errors, not silently omitted records.

Options/defaults/escaping and limits are shared with [objdump](mulib-objdump.md).
Symbols are limited in aggregate across symbol tables. ELF's existing extended
counts/index handling is reused. There is no instruction disassembly, relocation
dump/application, DWARF decoding or external tool invocation. See
[ELF reader](mulib-elf.md) for structural reader scope.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-elf-objdump.h`](../include/mulib-elf-objdump.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-elf-objdump.h
 * @brief ELF32/64 program, section, symbol and hex dumps using shared objdump.
 * Define MULIB_IMPL_ELF_OBJDUMP plus ELF/OBJDUMP dependencies. */
#ifndef MULIB_ELF_OBJDUMP_H
#define MULIB_ELF_OBJDUMP_H
#include "mulib-elf.h"
#include "mulib-objdump.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Atomic append; 1 success/0 malformed, limit, style or allocation error. */
int mulib_elf_objdump (const void *data, size_t size, const mulib_objdump_options_t *options, mulib_strbuf_t *out);
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
| Header | [`include/mulib-elf-objdump.h`](../include/mulib-elf-objdump.h) |
| Implementation switch | `MULIB_IMPL_ELF_OBJDUMP` |
| Repository-local includes | [mulib-elf](mulib-elf.md), [mulib-objdump](mulib-objdump.md) |
| Complete example | [`examples/modules/elf-objdump.c`](../examples/modules/elf-objdump.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/elf-objdump.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
