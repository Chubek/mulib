# lsh — MinHash band keys

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_LSH`.

Band hashes `signature[offset..offset+count)` using a deterministic byte-order
independent 64-bit hash. The caller must verify the band lies within the actual
signature; the API has no total-length argument. NULL signature or offset+count
overflow returns zero (which is also a possible valid hash). Count zero with a
valid pointer hashes an empty band. O(count), allocation-free. Use band number
plus key in a candidate table; collisions require signature/exact verification.
This helper does not store or retrieve candidates itself.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-lsh.h`](../include/mulib-lsh.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Band-key construction for locality-sensitive MinHash candidate retrieval. C11
 * single-header; define MULIB_IMPL_LSH once. */
#ifndef MULIB_LSH_H
#define MULIB_LSH_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  /* Deterministic hash of signature elements in band [offset,offset+count). */
  uint64_t mulib_lsh_band (const uint64_t *signature, size_t offset, size_t count);
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
| Header | [`include/mulib-lsh.h`](../include/mulib-lsh.h) |
| Implementation switch | `MULIB_IMPL_LSH` |
| Repository-local includes | None |
| Complete example | [`examples/modules/lsh.c`](../examples/modules/lsh.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/lsh.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
