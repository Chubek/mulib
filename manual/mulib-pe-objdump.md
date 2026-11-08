# PE dump adapter

[Manuals](README.md) · [Library index](index.md)

Include `mulib-pe-objdump.h`. Define `MULIB_IMPL_PE_OBJDUMP`,
`MULIB_IMPL_COFF_OBJDUMP`, `MULIB_IMPL_PE`, `MULIB_IMPL_COFF`, `MULIB_IMPL_ENDIAN`,
`MULIB_IMPL_OBJDUMP`, `MULIB_IMPL_TERMSTYLE`, `MULIB_IMPL_TERMINFO`,
`MULIB_IMPL_STRBUF`, `MULIB_IMPL_CSS`, `MULIB_IMPL_XML`, `MULIB_IMPL_UTF8`,
`MULIB_IMPL_VTERM`, `MULIB_IMPL_GRID`. [Example](../examples/modules/pe-objdump.c).

`mulib_pe_objdump(data, length, options, out)` opens PE32/PE32+ and atomically
appends a complete text dump. It returns 1 on success and 0 on malformed input,
limits, unavailable style capabilities or allocation failure. Existing output
remains unchanged on failure. No input ownership is taken.

HEADERS prints class/machine/characteristics, an `image` metadata record (address
is image base, offset is entry RVA, size is image size, flags is subsystem), and
nonempty data-directory records. Certificate directory 4 uses a file offset;
other directories are reported as RVAs, not decoded or resolved.
SECTIONS, SYMBOLS and CONTENTS reuse the COFF renderer. Symbols are COFF debug
symbols, **not DLL exports**. Contents include raw section padding because the
dump describes actual file data; virtual zero-fill is not synthesized.

See [shared options and escaping](mulib-objdump.md), [COFF adapter](mulib-coff-objdump.md)
and [PE reader scope](mulib-pe.md). No imports/exports, instructions, CLR, resources,
unwind data, relocation processing or signature validation are implemented.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-pe-objdump.h`](../include/mulib-pe-objdump.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-pe-objdump.h
 * @brief PE header, section, directory and COFF-symbol dumps over shared objdump.
 * Define MULIB_IMPL_PE_OBJDUMP plus PE/COFF_OBJDUMP dependencies. */
#ifndef MULIB_PE_OBJDUMP_H
#define MULIB_PE_OBJDUMP_H
#include "mulib-pe.h"
#include "mulib-coff-objdump.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Atomic append, 1 success/0 malformed, limit or OOM. No disassembly. */
int mulib_pe_objdump (const void *data, size_t size, const mulib_objdump_options_t *options, mulib_strbuf_t *out);
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
| Header | [`include/mulib-pe-objdump.h`](../include/mulib-pe-objdump.h) |
| Implementation switch | `MULIB_IMPL_PE_OBJDUMP` |
| Repository-local includes | [mulib-coff-objdump](mulib-coff-objdump.md), [mulib-pe](mulib-pe.md) |
| Complete example | [`examples/modules/pe-objdump.c`](../examples/modules/pe-objdump.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/pe-objdump.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
