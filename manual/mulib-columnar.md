# columnar — nullable numeric column

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_COLUMNAR`.

Append copies a finite double when `valid` is nonzero; otherwise stores null
as value zero/validity zero and ignores the supplied number. Return 1 on success,
0 on invalid input, allocation failure or overflow. Growth allocates both arrays
before publication, so failed append preserves the column. Append is amortized
O(1). Get is O(1), returns 0 out of range without changing outputs, and accepts
either output pointer as NULL. `free` releases both arrays and resets state.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-columnar.h`](../include/mulib-columnar.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Nullable column-major finite double vectors with atomic append. C11
 * single-header; define MULIB_IMPL_COLUMNAR once. */
#ifndef MULIB_COLUMNAR_H
#define MULIB_COLUMNAR_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    double *values;
    unsigned char *valid;
    size_t count, capacity;
  } mulib_columnar_t;
  /* Each append stores numeric value or null when valid==0. */
  int mulib_columnar_append (mulib_columnar_t *c, double value, int valid);
  int mulib_columnar_get (const mulib_columnar_t *c, size_t index, double *value,
                          int *valid);
  void mulib_columnar_free (mulib_columnar_t *c);
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
| Header | [`include/mulib-columnar.h`](../include/mulib-columnar.h) |
| Implementation switch | `MULIB_IMPL_COLUMNAR` |
| Repository-local includes | None |
| Complete example | [`examples/modules/columnar.c`](../examples/modules/columnar.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/columnar.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
