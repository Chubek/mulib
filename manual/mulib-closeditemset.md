# closeditemset — frequent closed intents

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_CLOSEDITEMSET`,
`MULIB_IMPL_NEXTCLOSURE`, and `MULIB_IMPL_FCA`.

Enumerate closed attribute sets in lectic order, retaining those whose extent
has at least `min_support` objects. Support zero includes empty extents; support
above the object count returns zero immediately. The callback receives masks
by value and returns nonzero to continue. The result counts callbacks including
a stopping callback; `SIZE_MAX` means invalid context/callback or count overflow.
Contexts must remain unchanged during traversal. No allocation; worst-case
enumeration is exponential in attributes.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-closeditemset.h`](../include/mulib-closeditemset.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Frequent closed intents of an FCA context with minimum support. C11
 * single-header; define MULIB_IMPL_CLOSEDITEMSET once. */
#ifndef MULIB_CLOSEDITEMSET_H
#define MULIB_CLOSEDITEMSET_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-nextclosure.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef int (*mulib_closeditemset_visit_fn) (uint64_t attributes,
                                               uint64_t object_extent, void *context);
  /* Exact closed-set enumeration; callbacks return nonzero to continue. */
  size_t mulib_closeditemset_enumerate (const mulib_fca_t *c, size_t min_support,
                                        mulib_closeditemset_visit_fn visit,
                                        void *context);
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
| Header | [`include/mulib-closeditemset.h`](../include/mulib-closeditemset.h) |
| Implementation switch | `MULIB_IMPL_CLOSEDITEMSET` |
| Repository-local includes | [mulib-nextclosure](mulib-nextclosure.md) |
| Complete example | [`examples/modules/closeditemset.c`](../examples/modules/closeditemset.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/closeditemset.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
