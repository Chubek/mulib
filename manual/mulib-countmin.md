# countmin — frequency sketch

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_COUNTMIN`.

Initialize zero state with positive width/depth; init returns 0 for invalid
sizes, already allocated state, overflow or allocation failure. Add accepts
binary keys (NULL only for zero length), incrementing one counter per row.
Counters saturate at UINT64_MAX. Estimate returns their minimum, never below
an inserted key's accumulated count up to saturation; collisions overestimate.
Invalid add returns 0, successful add 1; invalid estimate returns 0.
Hashing is deterministic and noncryptographic; no adversarial error bound is
promised. Storage is width × depth counters; operations cost O(depth × key bytes).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-countmin.h`](../include/mulib-countmin.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Deterministic Count-Min Sketch for estimated frequencies. C11 single-header;
 * define MULIB_IMPL_COUNTMIN once. */
#ifndef MULIB_COUNTMIN_H
#define MULIB_COUNTMIN_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t *cells;
    size_t width, depth;
  } mulib_countmin_t;
  int mulib_countmin_init (mulib_countmin_t *s, size_t width, size_t depth);
  int mulib_countmin_add (mulib_countmin_t *s, const void *key, size_t bytes,
                          uint64_t count);
  uint64_t mulib_countmin_estimate (const mulib_countmin_t *s, const void *key,
                                    size_t bytes);
  void mulib_countmin_free (mulib_countmin_t *s);
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
| Header | [`include/mulib-countmin.h`](../include/mulib-countmin.h) |
| Implementation switch | `MULIB_IMPL_COUNTMIN` |
| Repository-local includes | None |
| Complete example | [`examples/modules/countmin.c`](../examples/modules/countmin.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/countmin.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
