# docid — sortable document identifiers

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_DOCID`.

All uint64 values, including zero and UINT64_MAX, are valid IDs. Encode writes
exactly eight big-endian bytes; decode reads eight. Unsigned byte lexicographic
order matches numeric order. NULL output is a no-op; NULL input decodes to zero
and is indistinguishable from ID zero. No allocation, ID generation, or global
uniqueness tracking is performed. Both functions have constant cost.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-docid.h`](../include/mulib-docid.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Stable unsigned 64-bit document IDs and order-preserving big-endian
 * serialization. C11 single-header; define MULIB_IMPL_DOCID once. */
#ifndef MULIB_DOCID_H
#define MULIB_DOCID_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef uint64_t mulib_docid_t;
  void mulib_docid_encode (uint64_t id, unsigned char out[8]);
  uint64_t mulib_docid_decode (const unsigned char in[8]);
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
| Header | [`include/mulib-docid.h`](../include/mulib-docid.h) |
| Implementation switch | `MULIB_IMPL_DOCID` |
| Repository-local includes | None |
| Complete example | [`examples/modules/docid.c`](../examples/modules/docid.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/docid.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
