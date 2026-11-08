# binfileschema

[Manuals](README.md) · [Library index](index.md)

Borrowed fixed-record schema: array of named fields with byte offsets and widths
1..8. `validate` requires nonzero record size, unique nonempty names, bounded
fields and no overlaps. Gaps are permitted. No arrays, nesting, strings, native
padding assumptions or variable-length data model.

`get` validates schema and decodes a named unsigned LE field from a complete
record. Returns 1 success, 0 invalid/missing/truncated; value unchanged on error.
Schema/field names must remain live and immutable while used.
Switches: `MULIB_IMPL_BINFILESCHEMA`, `MULIB_IMPL_BYCUTILS`.
Example: `examples/modules/binfileschema.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-binfileschema.h`](../include/mulib-binfileschema.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-binfileschema.h
 * @brief Fixed-record schemas for named 1..8 byte little-endian integer
 * fields. Define MULIB_IMPL_BINFILESCHEMA plus BYCUTILS. Schemas borrow
 * immutable fields. */
#ifndef MULIB_BINFILESCHEMA_H
#define MULIB_BINFILESCHEMA_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_binfileschema_field
  {
    const char *name;
    size_t offset;
    unsigned width;
  } mulib_binfileschema_field_t;
  typedef struct mulib_binfileschema
  {
    const mulib_binfileschema_field_t *fields;
    size_t count, record_size;
  } mulib_binfileschema_t;
  /** @brief Require unique nonempty names, valid widths/ranges and no overlap.
   */
  int mulib_binfileschema_validate (const mulib_binfileschema_t *schema);
  /** @brief Decode named field from a complete record; 1 success, unchanged on
   * error. */
  int mulib_binfileschema_get (const mulib_binfileschema_t *schema,
                               const void *record, size_t size,
                               const char *name, uint64_t *value);
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
| Header | [`include/mulib-binfileschema.h`](../include/mulib-binfileschema.h) |
| Implementation switch | `MULIB_IMPL_BINFILESCHEMA` |
| Repository-local includes | [mulib-bycutils](mulib-bycutils.md) |
| Complete example | [`examples/modules/binfileschema.c`](../examples/modules/binfileschema.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/binfileschema.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
