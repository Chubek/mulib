# binfilebuilder

[Manuals](README.md) · [Library index](index.md)

`record` validates a fixed-record schema and appends one zero-padded record to an
initialized buffer. Values are uint64 in schema-field order; count must match,
all fields required and each value must fit its width. Returns 1 success,
0 invalid/range/allocation, without partial output. Schema/values are borrowed
only during call. Result bytes belong to the buffer; no file I/O.
The buffer invariant, total-length overflow and every field's value range are
checked before allocating record storage. Invalid values therefore cannot trigger
an allocation proportional to record size. Values may refer to the destination's
existing bytes because encoding completes in temporary storage before appending.

Switches: `MULIB_IMPL_BINFILEBUILDER`, `MULIB_IMPL_BINFILESCHEMA`,
`MULIB_IMPL_BYCUTILS`, `MULIB_IMPL_BUFFER`.
Example: `examples/modules/binfilebuilder.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-binfilebuilder.h`](../include/mulib-binfilebuilder.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-binfilebuilder.h
 * @brief Transactional fixed-schema record encoding into shared byte buffers.
 * Define MULIB_IMPL_BINFILEBUILDER plus BINFILESCHEMA, BYCUTILS and BUFFER. */
#ifndef MULIB_BINFILEBUILDER_H
#define MULIB_BINFILEBUILDER_H
#include "mulib-binfileschema.h"
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Append zero-padded record, values in schema field order. All
   * fields required; count must match. 1 success, 0 invalid/range/OOM, output
   * unchanged. */
  int mulib_binfilebuilder_record (mulib_buffer_t *out,
                                   const mulib_binfileschema_t *schema,
                                   const uint64_t *values, size_t count);
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
| Header | [`include/mulib-binfilebuilder.h`](../include/mulib-binfilebuilder.h) |
| Implementation switch | `MULIB_IMPL_BINFILEBUILDER` |
| Repository-local includes | [mulib-binfileschema](mulib-binfileschema.md), [mulib-buffer](mulib-buffer.md), [mulib-bycutils](mulib-bycutils.md) |
| Complete example | [`examples/modules/binfilebuilder.c`](../examples/modules/binfilebuilder.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/binfilebuilder.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
