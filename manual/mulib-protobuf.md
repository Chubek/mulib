# Protobuf wire fields

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_PROTOBUF`, `VARINT` and `BUFFER`. Wire types 0,1,2,5 are
supported; no groups or generated schema semantics. Bytes views borrow input.
Read returns 1 success, 0 truncation, -1 invalid, leaving outputs unchanged on
failure. Write atomically appends. ZigZag handles the full int64 range.
[Example](../examples/modules/protobuf.c), [contracts](storage-server.md).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-protobuf.h`](../include/mulib-protobuf.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-protobuf.h
 * @brief Schema-free Protobuf wire fields (varint/fixed64/bytes/fixed32).
 * Define MULIB_IMPL_PROTOBUF, VARINT and BUFFER. Groups are unsupported.
 * Read: 1 field, 0 truncated, -1 invalid. Failure unchanged. Write: 1/0,
 * failure-atomic append. No generated messages or implicit signed conversion.
 */
#ifndef MULIB_PROTOBUF_H
#define MULIB_PROTOBUF_H
#include "mulib-buffer.h"
#include "mulib-varint.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_protobuf_field
  {
    uint32_t number;
    unsigned wire;
    uint64_t value;
    const unsigned char *data;
    size_t length;
  } mulib_protobuf_field_t;
  int mulib_protobuf_read (const void *data, size_t length,
                           mulib_protobuf_field_t *out, size_t *consumed);
  int mulib_protobuf_write (mulib_buffer_t *out,
                            const mulib_protobuf_field_t *field);
  uint64_t mulib_protobuf_zigzag (int64_t value);
  int64_t mulib_protobuf_unzigzag (uint64_t value);
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
| Header | [`include/mulib-protobuf.h`](../include/mulib-protobuf.h) |
| Implementation switch | `MULIB_IMPL_PROTOBUF` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-varint](mulib-varint.md) |
| Complete example | [`examples/modules/protobuf.c`](../examples/modules/protobuf.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/protobuf.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
