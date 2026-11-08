# Mach-O lookup utilities

[Manuals](README.md) · [Library index](index.md)

Include `mulib-machoutils.h`; define `MULIB_IMPL_MACHOUTILS`, `MULIB_IMPL_MACHO`,
`MULIB_IMPL_ENDIAN`. [Example](../examples/modules/machoutils.c).

`mulib_machoutils_section(view, segment, name, index, out)` finds the first exact
section name in an exact segment. A NULL segment matches any segment; the optional
index is zero-based across all segment commands. Fixed-size file names are decoded
safely, including sixteen-character names without a NUL in the original bytes.

`mulib_machoutils_symbol(view, name, out)` finds an exact defined N_SECT or N_ABS
symbol, excluding STAB/debug and undefined/common symbols. It does not add/remove
leading underscores, demangle names, resolve reexports or interpret indirect
symbols. Names and sections are structurally checked during the search.

Return zero on success, `MULIB_MACHO_ENOTFOUND` when absent, or a Mach-O reader
error. Outputs remain unchanged on failure. No allocations or global state;
the input view borrows immutable file bytes. See [Mach-O reader](mulib-macho.md)
and [shared contracts](binary-formats.md).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-machoutils.h`](../include/mulib-machoutils.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-machoutils.h
 * @brief Exact Mach-O section and defined non-debug symbol lookup.
 * Define MULIB_IMPL_MACHOUTILS, MACHO and ENDIAN. */
#ifndef MULIB_MACHOUTILS_H
#define MULIB_MACHOUTILS_H
#include "mulib-macho.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief First exact section; NULL segment matches any segment. Optional index. */
int mulib_machoutils_section (const mulib_macho_t *v, const char *segment, const char *name, size_t *index, mulib_macho_section_t *out);
/** @brief First exact defined N_SECT/N_ABS symbol; no underscore rewriting. */
int mulib_machoutils_symbol (const mulib_macho_t *v, const char *name, mulib_macho_symbol_t *out);
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
| Header | [`include/mulib-machoutils.h`](../include/mulib-machoutils.h) |
| Implementation switch | `MULIB_IMPL_MACHOUTILS` |
| Repository-local includes | [mulib-macho](mulib-macho.md) |
| Complete example | [`examples/modules/machoutils.c`](../examples/modules/machoutils.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/machoutils.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
