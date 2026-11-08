# ELF builder

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_ELFBUILDER`, `OBJFILEBUILDER`, `OBJFILEWRITER`, `BINFILEWRITER`,
`BYCUTILS` and `BUFFER`. Reuses owned section accumulation and atomic ELF64
little-endian ET_REL writing. Names/data are copied; flags/alignment follow
objfilebuilder. No symbols/relocations/executable segments.
[Example](../examples/modules/elfbuilder.c), [contracts](storage-server.md).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-elfbuilder.h`](../include/mulib-elfbuilder.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-elfbuilder.h
 * @brief Owned ELF64 relocatable section builder reusing objfilebuilder.
 * Define MULIB_IMPL_ELFBUILDER and OBJFILEBUILDER dependencies.
 * No symbols, relocations or executable segments; see objfilewriter profile. */
#ifndef MULIB_ELFBUILDER_H
#define MULIB_ELFBUILDER_H
#include "mulib-objfilebuilder.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef mulib_objfilebuilder_t mulib_elfbuilder_t;
  int mulib_elfbuilder_add (mulib_elfbuilder_t *builder, const char *name,
                            const void *data, size_t size, size_t alignment,
                            uint64_t flags);
  int mulib_elfbuilder_finish (const mulib_elfbuilder_t *builder,
                               uint16_t machine, mulib_buffer_t *out);
  void mulib_elfbuilder_free (mulib_elfbuilder_t *builder);
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
| Header | [`include/mulib-elfbuilder.h`](../include/mulib-elfbuilder.h) |
| Implementation switch | `MULIB_IMPL_ELFBUILDER` |
| Repository-local includes | [mulib-objfilebuilder](mulib-objfilebuilder.md) |
| Complete example | [`examples/modules/elfbuilder.c`](../examples/modules/elfbuilder.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/elfbuilder.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
