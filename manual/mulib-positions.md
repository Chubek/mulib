# positions — sorted token locations

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_POSITIONS`.

Add maintains an owned ascending unique uint32 array. All positions, including
zero and UINT32_MAX, are valid; duplicate add succeeds without growing count.
Return 1 on success, 0 on invalid input, growth overflow or allocation failure.
Failures preserve contents. Binary search costs O(log n), insertion shifts O(n)
elements. Borrowed `items` pointers may expire on growth; free resets all state.
Use one list per document/token for phrasequery.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-positions.h`](../include/mulib-positions.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Owned sorted per-document token positions for phrase matching. C11
 * single-header; define MULIB_IMPL_POSITIONS once. */
#ifndef MULIB_POSITIONS_H
#define MULIB_POSITIONS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint32_t *items;
    size_t count, capacity;
  } mulib_positions_t;
  int mulib_positions_add (mulib_positions_t *p, uint32_t token_position);
  void mulib_positions_free (mulib_positions_t *p);
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
| Header | [`include/mulib-positions.h`](../include/mulib-positions.h) |
| Implementation switch | `MULIB_IMPL_POSITIONS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/positions.c`](../examples/modules/positions.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/positions.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
