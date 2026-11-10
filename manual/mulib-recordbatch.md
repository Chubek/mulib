# recordbatch — owned record collection

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_RECORDBATCH`,
`MULIB_IMPL_RECORD`, and `MULIB_IMPL_DATUM`.

Append deep-copies an initialized record, returning 1 on success or 0 on invalid
arguments/allocation/size failure without changing batch contents. The source
may be an existing record in the batch: cloning occurs before array growth.
Appending costs a record clone plus amortized O(1) array growth. Internal record
pointers may expire when the array grows. Free recursively releases all records
and zeros the owner; callers must not shallow-copy batch records out as owners.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-recordbatch.h`](../include/mulib-recordbatch.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Owned growable batch of deep-copied records. C11 single-header; define
 * MULIB_IMPL_RECORDBATCH once. */
#ifndef MULIB_RECORDBATCH_H
#define MULIB_RECORDBATCH_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-record.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    mulib_record_t *items;
    size_t count, capacity;
  } mulib_recordbatch_t;
  int mulib_recordbatch_append (mulib_recordbatch_t *b, const mulib_record_t *r);
  void mulib_recordbatch_free (mulib_recordbatch_t *b);
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
| Header | [`include/mulib-recordbatch.h`](../include/mulib-recordbatch.h) |
| Implementation switch | `MULIB_IMPL_RECORDBATCH` |
| Repository-local includes | [mulib-record](mulib-record.md) |
| Complete example | [`examples/modules/recordbatch.c`](../examples/modules/recordbatch.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/recordbatch.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
