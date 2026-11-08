# Row table

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_ROW_RDBMS`, `DOCUMENTDB`, `DBSCHEMA` and their implementation
closures. Set the borrowed schema before use. `put/get/delete` use uint64 ids,
owned JSON records, memtable sequences and tombstones; zero means write success.
No SQL or persistence. [Example](../examples/modules/row-rdbms.c),
[contracts](storage-server.md#storage-and-serialization).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-row-rdbms.h`](../include/mulib-row-rdbms.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-row-rdbms.h
 * @brief Schema-checked row-oriented in-memory table, not a SQL engine.
 * Define MULIB_IMPL_ROW_RDBMS, DBSCHEMA, DOCUMENTDB and their dependencies.
 * Zero initialize then assign schema (borrowed for table lifetime). */
#ifndef MULIB_ROW_RDBMS_H
#define MULIB_ROW_RDBMS_H
#include "mulib-dbschema.h"
#include "mulib-documentdb.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_row_rdbms
  {
    const mulib_dbschema_t *schema;
    mulib_documentdb_t documents;
  } mulib_row_rdbms_t;
  int mulib_row_rdbms_put (mulib_row_rdbms_t *table, uint64_t id,
                           const mulib_json_t *row, uint64_t sequence);
  mulib_json_t *mulib_row_rdbms_get (const mulib_row_rdbms_t *table,
                                     uint64_t id);
  int mulib_row_rdbms_delete (mulib_row_rdbms_t *table, uint64_t id,
                              uint64_t sequence);
  void mulib_row_rdbms_free (mulib_row_rdbms_t *table);
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
| Header | [`include/mulib-row-rdbms.h`](../include/mulib-row-rdbms.h) |
| Implementation switch | `MULIB_IMPL_ROW_RDBMS` |
| Repository-local includes | [mulib-dbschema](mulib-dbschema.md), [mulib-documentdb](mulib-documentdb.md) |
| Complete example | [`examples/modules/row-rdbms.c`](../examples/modules/row-rdbms.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/row-rdbms.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
