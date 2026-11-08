# Mach-O dump adapter

[Manuals](README.md) · [Library index](index.md)

Include `mulib-macho-objdump.h`. Define `MULIB_IMPL_MACHO_OBJDUMP`,
`MULIB_IMPL_MACHO`, `MULIB_IMPL_ENDIAN`, `MULIB_IMPL_OBJDUMP`,
`MULIB_IMPL_TERMSTYLE`, `MULIB_IMPL_TERMINFO`, `MULIB_IMPL_STRBUF`,
`MULIB_IMPL_CSS`, `MULIB_IMPL_XML`, `MULIB_IMPL_UTF8`, `MULIB_IMPL_VTERM`,
`MULIB_IMPL_GRID`.
[Example](../examples/modules/macho-objdump.c).

`mulib_macho_objdump(data, length, options, out)` accepts the checked reader's
thin Mach-O32/64 profile in either byte order. A complete dump is appended
atomically to an initialized `mulib_strbuf_t`; 1 success, 0 malformed input,
record limit, style or allocation failure. Inputs are borrowed only for the call.

HEADERS prints class, CPU, file type and bounded load-command metadata records.
Unknown commands are reported by numeric type/size without interpreting their
payload. SECTIONS prints section metadata; SYMBOLS prints all nlist records,
including debug and undefined symbols. CONTENTS adds bounded file-backed hex
data, skipping zero-fill section types. Names are safely escaped by the common
renderer; fixed-size sixteen-byte names are supported without a source NUL.

See [options/defaults/limits](mulib-objdump.md) and [reader scope](mulib-macho.md).
No fat-file traversal, machine-code disassembly, dyld fixups, relocation records,
debug information or signature verification. No external process is launched.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-macho-objdump.h`](../include/mulib-macho-objdump.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-macho-objdump.h
 * @brief Thin Mach-O command, section, symbol and hex dumps using shared objdump.
 * Define MULIB_IMPL_MACHO_OBJDUMP plus MACHO/OBJDUMP dependencies. */
#ifndef MULIB_MACHO_OBJDUMP_H
#define MULIB_MACHO_OBJDUMP_H
#include "mulib-macho.h"
#include "mulib-objdump.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Atomic append; 1 success, 0 malformed, limit, missing style or OOM. */
int mulib_macho_objdump (const void *data, size_t size, const mulib_objdump_options_t *options, mulib_strbuf_t *out);
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
| Header | [`include/mulib-macho-objdump.h`](../include/mulib-macho-objdump.h) |
| Implementation switch | `MULIB_IMPL_MACHO_OBJDUMP` |
| Repository-local includes | [mulib-macho](mulib-macho.md), [mulib-objdump](mulib-objdump.md) |
| Complete example | [`examples/modules/macho-objdump.c`](../examples/modules/macho-objdump.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/macho-objdump.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
