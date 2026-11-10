# nextclosure — lectic closure enumeration

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_NEXTCLOSURE`
and `MULIB_IMPL_FCA`.

First returns closure(empty). Next requires a closed current intent in the
context's universe and returns 1 with its lectic successor, or 0 for invalid
input/no successor without changing output. Input and output may use the same
scalar. Do not mutate the context mid-enumeration. Attribute indices determine
lectic order; it differs from numeric mask order. Empty contexts have one closed
intent. Each step costs O(attributes × objects); full enumeration can be
exponential. No allocation; use conceptlattice for bounded array collection.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-nextclosure.h`](../include/mulib-nextclosure.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Ganter NextClosure canonical enumeration of closed formal concepts. C11
 * single-header; define MULIB_IMPL_NEXTCLOSURE once. */
#ifndef MULIB_NEXTCLOSURE_H
#define MULIB_NEXTCLOSURE_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-fca.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /* First closed intent is closure(empty). Returns 1 if a lectically next intent
   * exists. */
  uint64_t mulib_nextclosure_first (const mulib_fca_t *c);
  int mulib_nextclosure_next (const mulib_fca_t *c, uint64_t current, uint64_t *next);
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
| Header | [`include/mulib-nextclosure.h`](../include/mulib-nextclosure.h) |
| Implementation switch | `MULIB_IMPL_NEXTCLOSURE` |
| Repository-local includes | [mulib-fca](mulib-fca.md) |
| Complete example | [`examples/modules/nextclosure.c`](../examples/modules/nextclosure.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/nextclosure.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
