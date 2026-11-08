# objfileinjector

[Manuals](README.md) · [Library index](index.md)

In-place, same-size replacement of a named ELF PROGBITS section, not process
injection. `replace` first validates the complete ELF file, name/type/range and
length, rejecting section ranges overlapping the declared ELF header (including
extensions), header tables, or another file-backed section. Zero-sized replacement
is a successful no-op after section/type checks. Then
memmove permits aliased replacement bytes. Returns 0 success, negative ELF error;
failure leaves bytes unchanged.

Does not resize sections, adjust offsets, update signatures/digests, relocate
code or change executable-memory protections. Caller owns writable object bytes.
Switches: `MULIB_IMPL_OBJFILEINJECTOR`, `MULIB_IMPL_ELFUTILS`, `MULIB_IMPL_ELF`,
`MULIB_IMPL_ENDIAN`. Example: `examples/modules/objfileinjector.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-objfileinjector.h`](../include/mulib-objfileinjector.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-objfileinjector.h
 * @brief In-place, size-preserving replacement of file-backed ELF sections.
 * Define MULIB_IMPL_OBJFILEINJECTOR plus ELFUTILS, ELF, ENDIAN. No process
 * injection. */
#ifndef MULIB_OBJFILEINJECTOR_H
#define MULIB_OBJFILEINJECTOR_H
#include "mulib-elfutils.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Validate ELF and replace an entire named section only when lengths
   * match. Reject NULL/NOBITS/metadata sections (only PROGBITS type 1),
   * header overlap and overlap with any other file-backed section.
   * 0 success, ELF error otherwise. memmove permits overlapping replacement.
   */
  int mulib_objfileinjector_replace (void *object, size_t size,
                                     const char *section, const void *data,
                                     size_t length);
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
| Header | [`include/mulib-objfileinjector.h`](../include/mulib-objfileinjector.h) |
| Implementation switch | `MULIB_IMPL_OBJFILEINJECTOR` |
| Repository-local includes | [mulib-elfutils](mulib-elfutils.md) |
| Complete example | [`examples/modules/objfileinjector.c`](../examples/modules/objfileinjector.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/objfileinjector.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
