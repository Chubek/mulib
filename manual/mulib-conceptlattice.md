# conceptlattice — bounded concept enumeration

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_CONCEPTLATTICE`,
`MULIB_IMPL_NEXTCLOSURE`, and `MULIB_IMPL_FCA`.

Enumerate intent/extent mask pairs in lectic intent order. The caller provides
capacity in concepts. `SIZE_MAX` reports invalid inputs or insufficient capacity
without writing any output; otherwise the returned count is positive, even for
the empty context (one empty concept). Two passes avoid partial publication.
`leq(a,b)` tests extent inclusion; compare concepts from the same context.
No allocation. Enumeration is potentially exponential; the first pass stops
as soon as capacity is exceeded. Use closeditemset callbacks for streaming.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-conceptlattice.h`](../include/mulib-conceptlattice.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Enumerate exact FCA concept intent/extent pairs with bounded capacity. C11
 * single-header; define MULIB_IMPL_CONCEPTLATTICE once. */
#ifndef MULIB_CONCEPTLATTICE_H
#define MULIB_CONCEPTLATTICE_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-nextclosure.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t intent, extent;
  } mulib_concept_t;
  /* Returns number of concepts, SIZE_MAX on insufficient capacity without output
   * mutation. */
  size_t mulib_conceptlattice_enumerate (const mulib_fca_t *c, mulib_concept_t *out,
                                         size_t capacity);
  /* a<=b in extent ordering, equivalently intent_b subset intent_a. */
  int mulib_conceptlattice_leq (mulib_concept_t a, mulib_concept_t b);
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
| Header | [`include/mulib-conceptlattice.h`](../include/mulib-conceptlattice.h) |
| Implementation switch | `MULIB_IMPL_CONCEPTLATTICE` |
| Repository-local includes | [mulib-nextclosure](mulib-nextclosure.md) |
| Complete example | [`examples/modules/conceptlattice.c`](../examples/modules/conceptlattice.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/conceptlattice.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
