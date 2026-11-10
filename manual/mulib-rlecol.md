# rlecol — unsigned run-length columns

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_RLECOL`.

Encode groups consecutive equal uint64 values into `{value,count}` runs and
returns run count. Decode expands positive-count runs and returns value count.
Either returns SIZE_MAX for invalid pointers, zero-length runs, overflow or
insufficient output capacity, without partial writes. Empty inputs accept NULL
with zero capacity and return zero. Adjacent equal runs are accepted by decode.
Input/output must not overlap. Encode is O(values), decode O(runs + values),
with no allocation. These are native structs, not a serialized wire format.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-rlecol.h`](../include/mulib-rlecol.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Run-length encoding of unsigned 64-bit column values. C11 single-header;
 * define MULIB_IMPL_RLECOL once. */
#ifndef MULIB_RLECOL_H
#define MULIB_RLECOL_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t value, count;
  } mulib_rlecol_run_t;
  /* Returns run count; SIZE_MAX invalid/capacity insufficiency, no partial writes on
   * capacity error. */
  size_t mulib_rlecol_encode (const uint64_t *values, size_t n,
                              mulib_rlecol_run_t *runs, size_t capacity);
  size_t mulib_rlecol_decode (const mulib_rlecol_run_t *runs, size_t n, uint64_t *out,
                              size_t capacity);
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
| Header | [`include/mulib-rlecol.h`](../include/mulib-rlecol.h) |
| Implementation switch | `MULIB_IMPL_RLECOL` |
| Repository-local includes | None |
| Complete example | [`examples/modules/rlecol.c`](../examples/modules/rlecol.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/rlecol.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
