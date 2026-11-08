# dbquery

[Manuals](README.md) · [Library index](index.md)

`scan` visits live records in unsigned-byte key order over [low,high), with a
callback limit. NULL high/zero length means no upper bound; an actual empty
upper key requires a non-NULL pointer. NULL low/zero length denotes the empty key.
Tombstones are skipped, limit zero visits none. Empty/reversed ranges visit none.

Callback borrows each record and returns nonzero to continue; never mutate/free
the table during a scan. Returned count includes a stopping callback; SIZE_MAX
means invalid arguments. No expression planner or secondary-index selection.
Switches: `MULIB_IMPL_DBQUERY`, `MULIB_IMPL_MEMTABLE`.
Example: `examples/modules/dbquery.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-dbquery.h`](../include/mulib-dbquery.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-dbquery.h
 * @brief Ordered half-open binary key scans over live memtable records.
 * Define MULIB_IMPL_DBQUERY and MULIB_IMPL_MEMTABLE. */
#ifndef MULIB_DBQUERY_H
#define MULIB_DBQUERY_H
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Callback borrows record; nonzero continues, zero stops. Never
   * mutate table. */
  typedef int (*mulib_dbquery_visit_fn) (const mulib_memtable_record_t *,
                                         void *);
  /** @brief Scan [low,high), skipping tombstones. NULL high means no upper
   * bound; NULL low with size zero is the empty key. limit=0 visits none.
   * Count includes the stopping callback. Invalid input returns SIZE_MAX. */
  size_t mulib_dbquery_scan (const mulib_memtable_t *t, const void *low,
                             size_t ln, const void *high, size_t hn,
                             size_t limit, mulib_dbquery_visit_fn visit,
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
| Header | [`include/mulib-dbquery.h`](../include/mulib-dbquery.h) |
| Implementation switch | `MULIB_IMPL_DBQUERY` |
| Repository-local includes | [mulib-memtable](mulib-memtable.md) |
| Complete example | [`examples/modules/dbquery.c`](../examples/modules/dbquery.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dbquery.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
