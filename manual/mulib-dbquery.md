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

`scan_where` additionally accepts a predicate and separate predicate/visit
contexts. NULL predicate accepts all live records; zero skips, nonzero accepts.
The limit counts accepted visits, not examined records. Tombstones never reach
either callback. Predicates must not mutate the table. Work is O(log n + scanned
records), with no allocation. Regression: `tests/test_data_patches.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-dbquery.h`](../include/mulib-dbquery.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-dbquery.h
 * @brief Patched ordered binary key scans with optional predicate pushdown.
 * Define MULIB_IMPL_DBQUERY and MULIB_IMPL_MEMTABLE in one translation unit. */
#ifndef MULIB_DBQUERY_H
#define MULIB_DBQUERY_H
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef int (*mulib_dbquery_visit_fn) (const mulib_memtable_record_t *, void *);
  typedef int (*mulib_dbquery_predicate_fn) (const mulib_memtable_record_t *, void *);
  size_t mulib_dbquery_scan (const mulib_memtable_t *table, const void *low,
                             size_t low_bytes, const void *high, size_t high_bytes,
                             size_t limit, mulib_dbquery_visit_fn visit, void *context);
  /* Predicate skips when it returns zero; NULL predicate accepts all nondeleted.
   * Count includes stopping visit. Does not mutate table. */
  size_t mulib_dbquery_scan_where (const mulib_memtable_t *table, const void *low,
                                   size_t low_bytes, const void *high,
                                   size_t high_bytes, size_t limit,
                                   mulib_dbquery_predicate_fn predicate,
                                   void *pred_context, mulib_dbquery_visit_fn visit,
                                   void *visit_context);
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
