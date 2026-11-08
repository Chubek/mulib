# PE lookup utilities

[Manuals](README.md) · [Library index](index.md)

Include `mulib-peutils.h`; define `MULIB_IMPL_PEUTILS`, `MULIB_IMPL_PE`,
`MULIB_IMPL_COFFUTILS`, `MULIB_IMPL_COFF`, `MULIB_IMPL_ENDIAN`.
[Example](../examples/modules/peutils.c).

`mulib_peutils_section(view, name, index, out)` finds the first exact named PE
section; the optional index is zero-based. Returned native section fields retain
the distinction between virtual size and padded raw file size.

`mulib_peutils_symbol(view, name, out)` searches the image's **COFF symbol table**,
not its export directory. Stripped images normally return ENOTFOUND. It skips
auxiliary, undefined and debug-only records, and includes defined absolute symbols.
DLL exports, forwarded exports and imported symbols are not resolved by this API.

Names are exact and case-sensitive. Return values are COFF result codes (zero
success, ENOTFOUND, or structural error). Outputs are unchanged on failure; no
allocation or ownership transfer occurs. The borrowed PE file must remain alive
and unchanged. See [PE reader](mulib-pe.md), [COFF lookup](mulib-coffutils.md),
and [shared contracts](binary-formats.md).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-peutils.h`](../include/mulib-peutils.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-peutils.h
 * @brief PE section and COFF debug-symbol lookup; not export resolution.
 * Define MULIB_IMPL_PEUTILS, PE, COFFUTILS, COFF and ENDIAN. */
#ifndef MULIB_PEUTILS_H
#define MULIB_PEUTILS_H
#include "mulib-pe.h"
#include "mulib-coffutils.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief First exact section, optional index; COFF result codes. */
int mulib_peutils_section (const mulib_pe_t *v, const char *name, size_t *index, mulib_pe_section_t *out);
/** @brief First defined COFF symbol; stripped images return ENOTFOUND. */
int mulib_peutils_symbol (const mulib_pe_t *v, const char *name, mulib_coff_symbol_t *out);
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
| Header | [`include/mulib-peutils.h`](../include/mulib-peutils.h) |
| Implementation switch | `MULIB_IMPL_PEUTILS` |
| Repository-local includes | [mulib-coffutils](mulib-coffutils.md), [mulib-pe](mulib-pe.md) |
| Complete example | [`examples/modules/peutils.c`](../examples/modules/peutils.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/peutils.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
