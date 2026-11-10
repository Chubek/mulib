# phrasequery — exact positional phrases

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_PHRASEQUERY`.
Enable `MULIB_IMPL_POSITIONS` to construct owned position lists.

Each input list contains sorted unique token positions for one phrase token in
the same document. Match returns 1 if some start p has position p+i in every
list i, otherwise 0. NULL/zero token count or count > UINT32_MAX returns 0.
No slop or cross-document matching is supported; wraparound positions cannot
match. A single nonempty list matches. For every first-token candidate, the
other lists are binary-searched. No allocations or mutations occur.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-phrasequery.h`](../include/mulib-phrasequery.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Ordered phrase matching over positional posting arrays. C11 single-header;
 * define MULIB_IMPL_PHRASEQUERY once. */
#ifndef MULIB_PHRASEQUERY_H
#define MULIB_PHRASEQUERY_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-positions.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /* positions[i] are sorted token locations of phrase token i; exact ordered phrase. */
  int mulib_phrasequery_match (const mulib_positions_t *positions, size_t token_count);
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
| Header | [`include/mulib-phrasequery.h`](../include/mulib-phrasequery.h) |
| Implementation switch | `MULIB_IMPL_PHRASEQUERY` |
| Repository-local includes | [mulib-positions](mulib-positions.md) |
| Complete example | [`examples/modules/phrasequery.c`](../examples/modules/phrasequery.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/phrasequery.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
