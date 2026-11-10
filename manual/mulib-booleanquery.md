# booleanquery — postings set operations

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_BOOLEANQUERY`
and `MULIB_IMPL_POSTINGS`.

Inputs are sorted unique postings built with the postings API. AND intersects,
OR unions, and NOT computes left minus right. Shared documents retain the larger
frequency; one-sided documents retain their original frequency. Output must be
zero-initialized and distinct from both inputs, which may be the same object.
Return 1 on success or 0 on invalid arguments/allocation or size overflow.
Failure preserves all objects. Free the resulting postings normally.
The merge is O(a.count + b.count), with O(output bound) allocated storage.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-booleanquery.h`](../include/mulib-booleanquery.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Linear-time postings intersection/union/difference on sorted documents. C11
 * single-header; define MULIB_IMPL_BOOLEANQUERY once. */
#ifndef MULIB_BOOLEANQUERY_H
#define MULIB_BOOLEANQUERY_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-postings.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum
  {
    MULIB_BOOLEAN_AND,
    MULIB_BOOLEAN_OR,
    MULIB_BOOLEAN_NOT
  } mulib_boolean_op_t;
  /* Writes a fresh output; requires zero initialized out distinct from inputs. */
  int mulib_booleanquery_apply (const mulib_postings_t *a, const mulib_postings_t *b,
                                mulib_boolean_op_t op, mulib_postings_t *out);
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
| Header | [`include/mulib-booleanquery.h`](../include/mulib-booleanquery.h) |
| Implementation switch | `MULIB_IMPL_BOOLEANQUERY` |
| Repository-local includes | [mulib-postings](mulib-postings.md) |
| Complete example | [`examples/modules/booleanquery.c`](../examples/modules/booleanquery.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/booleanquery.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
