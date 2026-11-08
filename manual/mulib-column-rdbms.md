# Column table

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_COLUMN_RDBMS`; no MuLIB dependencies. Zero-initialize and set
`columns`. Atomic append accepts finite doubles only. Column views remain valid
until mutation/free. Dimensions must not change while live. No SQL/nulls.
[Example](../examples/modules/column-rdbms.c), [contracts](storage-server.md).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-column-rdbms.h`](../include/mulib-column-rdbms.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-column-rdbms.h
 * @brief Dense column-oriented finite-double table with atomic row append.
 * Define MULIB_IMPL_COLUMN_RDBMS. No SQL, nulls or persistence. Zero
 * initialize, set nonzero columns before first append; never change columns
 * while live. */
#ifndef MULIB_COLUMN_RDBMS_H
#define MULIB_COLUMN_RDBMS_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_column_rdbms
  {
    double *data;
    size_t columns, rows;
  } mulib_column_rdbms_t;
  int mulib_column_rdbms_append (mulib_column_rdbms_t *table, const double *row,
                                 size_t count);
  /** Borrow contiguous column of rows elements until mutation/free. */
  const double *mulib_column_rdbms_column (const mulib_column_rdbms_t *table,
                                           size_t column);
  void mulib_column_rdbms_free (mulib_column_rdbms_t *table);
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
| Header | [`include/mulib-column-rdbms.h`](../include/mulib-column-rdbms.h) |
| Implementation switch | `MULIB_IMPL_COLUMN_RDBMS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/column-rdbms.c`](../examples/modules/column-rdbms.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/column-rdbms.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
