# Document records

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_DOCUMENTDB`, `MEMTABLE`, `JSON`, `PARSEC`, `STRBUF` and `UTF8`.
Binary-keyed object documents are copied as JSON text. Gets return independent
owned values; free them with `mulib_json_destroy`. Writes use memtable result
codes, sequences and tombstones. No disk engine or query language.
[Example](../examples/modules/documentdb.c), [contracts](storage-server.md).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-documentdb.h`](../include/mulib-documentdb.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-documentdb.h
 * @brief In-memory binary-keyed JSON document records over memtable.
 * Define MULIB_IMPL_DOCUMENTDB, MEMTABLE, JSON and JSON dependencies.
 * Zero initialize; no persistence, query language or concurrency. */
#ifndef MULIB_DOCUMENTDB_H
#define MULIB_DOCUMENTDB_H
#include "mulib-json.h"
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_documentdb
  {
    mulib_memtable_t records;
  } mulib_documentdb_t;
  /** Copy object as JSON text. Same sequence/tombstone rules as memtable. */
  int mulib_documentdb_put (mulib_documentdb_t *db, const void *key,
                            size_t key_length, const mulib_json_t *document,
                            uint64_t sequence);
  /** Independent owned object, or NULL for absent/deleted/error. */
  mulib_json_t *mulib_documentdb_get (const mulib_documentdb_t *db,
                                      const void *key, size_t key_length);
  int mulib_documentdb_delete (mulib_documentdb_t *db, const void *key,
                               size_t key_length, uint64_t sequence);
  void mulib_documentdb_free (mulib_documentdb_t *db);
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
| Header | [`include/mulib-documentdb.h`](../include/mulib-documentdb.h) |
| Implementation switch | `MULIB_IMPL_DOCUMENTDB` |
| Repository-local includes | [mulib-json](mulib-json.md), [mulib-memtable](mulib-memtable.md) |
| Complete example | [`examples/modules/documentdb.c`](../examples/modules/documentdb.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/documentdb.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
