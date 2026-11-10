# galois — context derivations

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_GALOIS` and
`MULIB_IMPL_FCA`.

`objects` is the FCA extent operation; `attributes` is its intent operation.
`object_closure` computes extent(intent(objects)). Masks use the context's
0–64 indexed universe, with out-of-range bits ignored and zero returned for
invalid contexts. Empty selections use vacuous intersection semantics, as in
[fca](mulib-fca.md). Valid closures are extensive, monotone and idempotent.
All operations are allocation-free and O(objects).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-galois.h`](../include/mulib-galois.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: FCA Galois derivation aliases with extent and intent closure. C11
 * single-header; define MULIB_IMPL_GALOIS once. */
#ifndef MULIB_GALOIS_H
#define MULIB_GALOIS_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-fca.h"
#ifdef __cplusplus
extern "C"
{
#endif
  uint64_t mulib_galois_objects (const mulib_fca_t *c, uint64_t attributes);
  uint64_t mulib_galois_attributes (const mulib_fca_t *c, uint64_t objects);
  uint64_t mulib_galois_object_closure (const mulib_fca_t *c, uint64_t objects);
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
| Header | [`include/mulib-galois.h`](../include/mulib-galois.h) |
| Implementation switch | `MULIB_IMPL_GALOIS` |
| Repository-local includes | [mulib-fca](mulib-fca.md) |
| Complete example | [`examples/modules/galois.c`](../examples/modules/galois.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/galois.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
