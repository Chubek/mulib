# latticequery — concept predicates

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_LATTICEQUERY`.
The concept type comes from conceptlattice; enable its FCA dependencies only
if also calling concept enumeration (see the complete example).

Filter requires every requested attribute in the intent and, when `any_objects`
is nonzero, at least one requested object in the extent. Zero object mask means
no object predicate. Returns matching input indices in input order, or SIZE_MAX
for invalid pointer/capacity. A first pass ensures output remains untouched on
error. Empty ranges allow NULL pointers. O(count), allocation-free; output must
not overlap the input concept array.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-latticequery.h`](../include/mulib-latticequery.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Attribute/extent predicate enumeration over concept-lattice results. C11
 * single-header; define MULIB_IMPL_LATTICEQUERY once. */
#ifndef MULIB_LATTICEQUERY_H
#define MULIB_LATTICEQUERY_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-conceptlattice.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /* Return matching concept indices, SIZE_MAX for insufficient output capacity. */
  size_t mulib_latticequery_filter (const mulib_concept_t *concepts, size_t count,
                                    uint64_t all_attributes, uint64_t any_objects,
                                    size_t *indices, size_t capacity);
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
| Header | [`include/mulib-latticequery.h`](../include/mulib-latticequery.h) |
| Implementation switch | `MULIB_IMPL_LATTICEQUERY` |
| Repository-local includes | [mulib-conceptlattice](mulib-conceptlattice.md) |
| Complete example | [`examples/modules/latticequery.c`](../examples/modules/latticequery.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/latticequery.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
