# COFF dump adapter

[Manuals](README.md) · [Library index](index.md)

Include `mulib-coff-objdump.h`. Define `MULIB_IMPL_COFF_OBJDUMP`, `MULIB_IMPL_COFF`,
`MULIB_IMPL_ENDIAN`, `MULIB_IMPL_OBJDUMP`, `MULIB_IMPL_TERMSTYLE`,
`MULIB_IMPL_TERMINFO`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_CSS`, `MULIB_IMPL_XML`,
`MULIB_IMPL_UTF8`, `MULIB_IMPL_VTERM`, `MULIB_IMPL_GRID`.
[Example](../examples/modules/coff-objdump.c).

`mulib_coff_objdump(data, length, options, out)` opens the checked COFF reader
then appends shared-format text atomically, returning 1/0. `objdump_view` is the
same renderer for an already validated COFF view, including the view embedded
in PE. Views and backing bytes must remain unchanged during rendering.

HEADERS prints COFF machine/characteristics (COFF alone has no universal bitness
field, so bits=0). SECTIONS prints native section fields. SYMBOLS prints primary
records only, skipping auxiliaries while retaining their raw table indexes.
CONTENTS adds bounded file-backed hex dumps; BSS without file data is not read.
Long and short names are resolved and escaped; malformed names fail the dump.

[Shared options](mulib-objdump.md) control limits and optional terminfo headings.
Record-limit, parse, style and allocation failures leave existing output unchanged.
No instruction disassembly, auxiliary decoding, relocation rendering, archive
iteration or external tools are used. See [COFF profile](mulib-coff.md).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-coff-objdump.h`](../include/mulib-coff-objdump.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-coff-objdump.h
 * @brief COFF metadata, symbols and section hex dumps using shared objdump tools.
 * Define MULIB_IMPL_COFF_OBJDUMP plus COFF/OBJDUMP dependencies. */
#ifndef MULIB_COFF_OBJDUMP_H
#define MULIB_COFF_OBJDUMP_H
#include "mulib-coff.h"
#include "mulib-objdump.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Append a complete dump atomically; 1 success, 0 malformed/limit/OOM.
 * Auxiliary symbols and zero-fill bytes are not printed as real records/data. */
int mulib_coff_objdump (const void *data, size_t size, const mulib_objdump_options_t *options, mulib_strbuf_t *out);
/** @brief Same renderer for an already opened view, also used by PE. */
int mulib_coff_objdump_view (const mulib_coff_t *v, const mulib_objdump_options_t *options, mulib_strbuf_t *out);
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
| Header | [`include/mulib-coff-objdump.h`](../include/mulib-coff-objdump.h) |
| Implementation switch | `MULIB_IMPL_COFF_OBJDUMP` |
| Repository-local includes | [mulib-coff](mulib-coff.md), [mulib-objdump](mulib-objdump.md) |
| Complete example | [`examples/modules/coff-objdump.c`](../examples/modules/coff-objdump.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/coff-objdump.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
