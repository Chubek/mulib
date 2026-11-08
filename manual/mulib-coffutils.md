# COFF lookup utilities

[Manuals](README.md) · [Library index](index.md)

Include `mulib-coffutils.h`; define `MULIB_IMPL_COFFUTILS`, `MULIB_IMPL_COFF`,
`MULIB_IMPL_ENDIAN`. [Example](../examples/modules/coffutils.c).

`mulib_coffutils_section(view, name, index, out)` finds the first exact section
name, resolving short and long names through the checked reader. `index` may be
NULL; `out` is required. The name argument is a NUL-terminated C string.

`mulib_coffutils_symbol(view, name, out)` finds the first exact primary symbol
with a positive section number or the absolute section number -1. Undefined and
debug-only symbols are ignored; auxiliary entries are skipped, not interpreted.
There is no demangling, case folding, linker decoration rewriting or relocation.

Both return zero on success, `MULIB_COFF_ENOTFOUND` if absent, or the reader's
structural error. Outputs (including optional section index) are unchanged on
failure. Malformed encountered names are errors rather than silently skipped.
No allocations. Inputs must be valid borrowed [COFF views](mulib-coff.md).
See also [shared ownership/profile contracts](binary-formats.md).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-coffutils.h`](../include/mulib-coffutils.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-coffutils.h
 * @brief Exact section and defined-primary-symbol lookup over checked COFF.
 * Define MULIB_IMPL_COFFUTILS, COFF and ENDIAN. */
#ifndef MULIB_COFFUTILS_H
#define MULIB_COFFUTILS_H
#include "mulib-coff.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief First exact section; optional index, required out, atomic on error. */
int mulib_coffutils_section (const mulib_coff_t *v, const char *name, size_t *index, mulib_coff_section_t *out);
/** @brief First named defined symbol (including absolute); skips auxiliaries. */
int mulib_coffutils_symbol (const mulib_coff_t *v, const char *name, mulib_coff_symbol_t *out);
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
| Header | [`include/mulib-coffutils.h`](../include/mulib-coffutils.h) |
| Implementation switch | `MULIB_IMPL_COFFUTILS` |
| Repository-local includes | [mulib-coff](mulib-coff.md) |
| Complete example | [`examples/modules/coffutils.c`](../examples/modules/coffutils.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/coffutils.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
