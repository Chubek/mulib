# AMD64

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_AMD64` and the [assembler](mulib-assembler.md) dependency closure.
Zero-initialize the context, call `mulib_amd64_init`, emit typed instructions and
free with `mulib_amd64_free`. It owns generated dsymspec metadata; it is not a
second encoder or a text assembler. See [example](../examples/modules/amd64.c)
and [shared contracts](storage-server.md).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-amd64.h`](../include/mulib-amd64.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-amd64.h
 * @brief Owned AMD64 metadata assembler convenience context.
 * Define MULIB_IMPL_AMD64 and all ASSEMBLER dependencies. Encodings and the
 * supported register/immediate/branch subset come from dsymspec, not a second
 * instruction table. Zero initialize; free before reinitializing. */
#ifndef MULIB_AMD64_H
#define MULIB_AMD64_H
#include "mulib-assembler.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_amd64
  {
    mulib_dsymspec_t *specification;
  } mulib_amd64_t;
  int mulib_amd64_init (mulib_amd64_t *context);
  void mulib_amd64_free (mulib_amd64_t *context);
  int mulib_amd64_emit (const mulib_amd64_t *context, mulib_emitter_t *image,
                        mulib_linker_t *linker, const char *mnemonic,
                        const mulib_assembler_operand_t *operands, size_t count,
                        mulib_assembler_error_t *error);
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
| Header | [`include/mulib-amd64.h`](../include/mulib-amd64.h) |
| Implementation switch | `MULIB_IMPL_AMD64` |
| Repository-local includes | [mulib-assembler](mulib-assembler.md) |
| Complete example | [`examples/modules/amd64.c`](../examples/modules/amd64.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/amd64.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
