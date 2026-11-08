# COFF builder

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_COFFBUILDER`, `MULIB_IMPL_BYCUTILS`, `MULIB_IMPL_BUFFER`,
`MULIB_IMPL_BINFILEWRITER`, `MULIB_IMPL_OBJFILEWRITER`,
`MULIB_IMPL_OBJFILEBUILDER`. `mulib_coffbuilder_text` atomically appends a
deterministic AMD64 COFF object with one read/execute .text section aligned to
16 bytes. No symbols/relocations. Input can alias output.
[Example](../examples/modules/coffbuilder.c), [contracts](storage-server.md).

For multiple sections zero-initialize `mulib_coffbuilder_t`, then call `add`,
`finish(builder, native_machine, out)` and `free`. Add copies unique 1..8-byte
names and payloads, with power-of-two alignment <=8192 and shared WRITE/ALLOC/EXEC
flags 1/2/4. Empty payloads may be NULL. Finish supports up to 32767 sections,
requires nonzero machine (AMD64=0x8664), and limits total size to UINT32_MAX.
Names starting with `/` are reserved for COFF long-name references and rejected.
No long-name table, symbols, relocation records, bigobj or archive writer.
Results are 1/0, failures leave output unchanged, finish does not consume builder.
See [binary format contracts](binary-formats.md).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-coffbuilder.h`](../include/mulib-coffbuilder.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-coffbuilder.h
 * @brief Deterministic AMD64 COFF object with one read/execute .text section.
 * Define MULIB_IMPL_COFFBUILDER plus OBJFILEBUILDER, OBJFILEWRITER,
 * BINFILEWRITER, BYCUTILS and BUFFER. No symbols/relocations.
 * Atomic append; input may alias output; 1 success, 0 invalid/OOM/overflow. */
#ifndef MULIB_COFFBUILDER_H
#define MULIB_COFFBUILDER_H
#include "mulib-buffer.h"
#include "mulib-bycutils.h"
#include "mulib-objfilebuilder.h"
#include "mulib-binfilewriter.h"
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_coffbuilder_text (mulib_buffer_t *out, const void *code,
                              size_t length);
  typedef mulib_objfilebuilder_t mulib_coffbuilder_t;
  /** @brief Copy a section with a 1..8 byte name, alignment <=8192 and
   * WRITE/ALLOC/EXEC flags 1/2/4. Initialize builder to zero. */
  int mulib_coffbuilder_add (mulib_coffbuilder_t *b, const char *name,
      const void *data, size_t size, size_t alignment, uint64_t flags);
  /** @brief Atomically append a multi-section COFF object; no symbols/relocations. */
  int mulib_coffbuilder_finish (const mulib_coffbuilder_t *b, uint16_t machine, mulib_buffer_t *out);
  /** @brief Free owned names/bytes and reset; NULL allowed. */
  void mulib_coffbuilder_free (mulib_coffbuilder_t *b);
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
| Header | [`include/mulib-coffbuilder.h`](../include/mulib-coffbuilder.h) |
| Implementation switch | `MULIB_IMPL_COFFBUILDER` |
| Repository-local includes | [mulib-binfilewriter](mulib-binfilewriter.md), [mulib-buffer](mulib-buffer.md), [mulib-bycutils](mulib-bycutils.md), [mulib-objfilebuilder](mulib-objfilebuilder.md) |
| Complete example | [`examples/modules/coffbuilder.c`](../examples/modules/coffbuilder.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/coffbuilder.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
