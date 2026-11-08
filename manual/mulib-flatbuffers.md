# flatbuffers

[Manuals](README.md) · [Library index](index.md)

Borrowed checked table views, not a FlatBuffers implementation/compiler.
`root` opens a non-size-prefixed LE32 root offset; `table` validates the signed
vtable distance, vtable/object lengths and bounds (including shared forward
vtables), rejecting overlapping vtable/object ranges. `uint` reads a present field of width 1..8; `string` follows a forward
offset, checking length and NUL termination. Returns 1 valid/found, 0 malformed,
absent or invalid; outputs unchanged on failure. Input must remain immutable.
Field access also checks the view's full ranges before adding offsets. String
offsets must skip their complete four-byte offset field.

No schema/default interpretation, vectors/unions/nested table accessors, writer,
UTF-8 checking or whole-buffer verification. Caller chooses correct scalar width.
Do not treat local field checks as validation of every reachable object.
Switches: `MULIB_IMPL_FLATBUFFERS`, `MULIB_IMPL_BYCUTILS`, `MULIB_IMPL_BINFILE`.
Example: `examples/modules/flatbuffers.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-flatbuffers.h`](../include/mulib-flatbuffers.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-flatbuffers.h
 * @brief Bounds-checked FlatBuffers table/scalar/string views, not a schema
 * compiler. Define MULIB_IMPL_FLATBUFFERS and MULIB_IMPL_BYCUTILS once.
 * Borrowed, immutable bytes. */
#ifndef MULIB_FLATBUFFERS_H
#define MULIB_FLATBUFFERS_H
#include "mulib-binfile.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_flatbuffers_table
  {
    mulib_binfile_t file;
    size_t table, vtable, object_size, vtable_size;
  } mulib_flatbuffers_table_t;
  /** @brief Open table at byte offset; signed vtable offsets may be
   * forward/backward. 1 success, 0 malformed/invalid. No whole-schema
   * verification is claimed. */
  int mulib_flatbuffers_table (const void *data, size_t size, size_t offset,
                               mulib_flatbuffers_table_t *out);
  /** @brief Open root from LE32 root offset; size-prefixed buffers not
   * supported. */
  int mulib_flatbuffers_root (const void *data, size_t size,
                              mulib_flatbuffers_table_t *out);
  /** @brief Scalar field id, width 1..8; 1 found, 0 absent/invalid. */
  int mulib_flatbuffers_uint (const mulib_flatbuffers_table_t *table,
                              unsigned field, unsigned width, uint64_t *out);
  /** @brief String field, checking forward offset, length and terminator. 1
   * found, 0 absent/malformed. UTF-8 is not checked; returned length excludes
   * terminator. */
  int mulib_flatbuffers_string (const mulib_flatbuffers_table_t *table,
                                unsigned field, mulib_binfile_t *out);
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
| Header | [`include/mulib-flatbuffers.h`](../include/mulib-flatbuffers.h) |
| Implementation switch | `MULIB_IMPL_FLATBUFFERS` |
| Repository-local includes | [mulib-binfile](mulib-binfile.md), [mulib-bycutils](mulib-bycutils.md) |
| Complete example | [`examples/modules/flatbuffers.c`](../examples/modules/flatbuffers.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/flatbuffers.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
