# objfilewriter

[Manuals](README.md) · [Library index](index.md)

`write` appends a deterministic ELF64 little-endian ET_REL object to a buffer.
Borrowed section inputs supply unique nonempty names, bytes, power-of-two
alignment and WRITE/ALLOC/EXECINSTR flags only. `.shstrtab` is reserved. Writer
emits PROGBITS sections, null section and section-name table; machine is supplied
as ELF e_machine (62 for x86-64). At most 65277 user sections.

Returns 1 success, 0 invalid/overflow/OOM; complete object built temporarily so
failure preserves output and inputs may alias it. No symbols, relocation tables,
program headers, NOBITS or extended counts. Never executes bytes.
Switches: `MULIB_IMPL_OBJFILEWRITER`, `MULIB_IMPL_BINFILEWRITER`,
`MULIB_IMPL_BYCUTILS`, `MULIB_IMPL_BUFFER`.
Example: `examples/modules/objfilewriter.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-objfilewriter.h`](../include/mulib-objfilewriter.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-objfilewriter.h
 * @brief Deterministic ELF64 little-endian ET_REL writer for PROGBITS
 * sections. Define MULIB_IMPL_OBJFILEWRITER plus BINFILEWRITER, BYCUTILS and
 * BUFFER. No program headers, symbols, relocations, extended section counts or
 * execution. */
#ifndef MULIB_OBJFILEWRITER_H
#define MULIB_OBJFILEWRITER_H
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Borrowed section input. Name nonempty/unique, not .shstrtab.
   * alignment is power-of-two; flags restricted to WRITE/ALLOC/EXECINSTR (bits
   * 0..2). */
  typedef struct mulib_objfilewriter_section
  {
    const char *name;
    const void *data;
    size_t size, alignment;
    uint64_t flags;
  } mulib_objfilewriter_section_t;
  /** @brief Append a complete object atomically; machine is ELF e_machine
   * (e.g. 62). 1 success, 0 invalid/overflow/OOM. Input may alias output
   * (built in temporary). */
  int mulib_objfilewriter_write (mulib_buffer_t *out, uint16_t machine,
                                 const mulib_objfilewriter_section_t *sections,
                                 size_t count);
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
| Header | [`include/mulib-objfilewriter.h`](../include/mulib-objfilewriter.h) |
| Implementation switch | `MULIB_IMPL_OBJFILEWRITER` |
| Repository-local includes | [mulib-binfilewriter](mulib-binfilewriter.md), [mulib-buffer](mulib-buffer.md), [mulib-bycutils](mulib-bycutils.md) |
| Complete example | [`examples/modules/objfilewriter.c`](../examples/modules/objfilewriter.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/objfilewriter.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
