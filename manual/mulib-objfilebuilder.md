# objfilebuilder

[Manuals](README.md) · [Library index](index.md)

Owned section builder over the focused ELF64 writer. Zero initialize; `add`
copies a unique section's name/data and validates flags/alignment. Returns 1
success, 0 invalid/duplicate/limit/OOM, preserving prior sections. Inputs may
alias existing sections because copies precede publication.

`finish` appends a complete object without consuming the builder; `free` releases
all names/bytes and resets. Same PROGBITS-only limits as object writer; no symbol
or relocation construction. Do not copy live builder ownership by assignment.
Switches: `MULIB_IMPL_OBJFILEBUILDER`, `MULIB_IMPL_OBJFILEWRITER`,
`MULIB_IMPL_BINFILEWRITER`, `MULIB_IMPL_BYCUTILS`, `MULIB_IMPL_BUFFER`.
Example: `examples/modules/objfilebuilder.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-objfilebuilder.h`](../include/mulib-objfilebuilder.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-objfilebuilder.h
 * @brief Owned section builder for the focused ELF64 object writer.
 * Define MULIB_IMPL_OBJFILEBUILDER and all OBJFILEWRITER dependencies. */
#ifndef MULIB_OBJFILEBUILDER_H
#define MULIB_OBJFILEBUILDER_H
#include "mulib-objfilewriter.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_objfilebuilder
  {
    mulib_objfilewriter_section_t *sections;
    size_t count;
  } mulib_objfilebuilder_t;
  /** @brief Zero init. Copy a unique section; 1 success, failure unchanged. */
  int mulib_objfilebuilder_add (mulib_objfilebuilder_t *b, const char *name,
                                const void *data, size_t size,
                                size_t alignment, uint64_t flags);
  /** @brief Append complete ELF64 object without consuming builder. */
  int mulib_objfilebuilder_finish (const mulib_objfilebuilder_t *b,
                                   uint16_t machine, mulib_buffer_t *out);
  /** @brief Release all section names/bytes and reset. */
  void mulib_objfilebuilder_free (mulib_objfilebuilder_t *b);
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
| Header | [`include/mulib-objfilebuilder.h`](../include/mulib-objfilebuilder.h) |
| Implementation switch | `MULIB_IMPL_OBJFILEBUILDER` |
| Repository-local includes | [mulib-objfilewriter](mulib-objfilewriter.md) |
| Complete example | [`examples/modules/objfilebuilder.c`](../examples/modules/objfilebuilder.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/objfilebuilder.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
