# sql

[Manuals](README.md) · [Library index](index.md)

Supported single statements only:

```sql
INSERT INTO kv VALUES ('key','value');
SELECT value FROM kv WHERE key='key';
DELETE FROM kv WHERE key='key';
```

Keywords are ASCII case-insensitive; one trailing semicolon is optional.
Whitespace is space/tab/CR/LF; literals escape quotes with `''`. Embedded NUL,
comments, multiple statements, other tables/columns, joins/expressions and
unsupported syntax fail before mutation. INSERT upserts. SELECT appends raw bytes
to a required initialized buffer; results are not C strings. DELETE tombstones.
Returns 0 success, 1 missing SELECT/DELETE, negative syntax/storage/OOM. Optional
error offset is the first unparsed byte on syntax error, SIZE_MAX otherwise.

Switches: `MULIB_IMPL_SQL`, `MULIB_IMPL_MEMTABLE`, `MULIB_IMPL_STRBUF`,
`MULIB_IMPL_BUFFER`. Example: `examples/modules/sql.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-sql.h`](../include/mulib-sql.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-sql.h
 * @brief Focused SQL key/value statements over a shared memtable, not general
 * SQL. Define MULIB_IMPL_SQL plus MEMTABLE, STRBUF and BUFFER. See manual for
 * grammar. */
#ifndef MULIB_SQL_H
#define MULIB_SQL_H
#include "mulib-buffer.h"
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Execute SELECT value FROM kv WHERE key='k'; INSERT INTO kv VALUES
   * ('k','v'); DELETE FROM kv WHERE key='k'. Keywords case-insensitive; quotes
   * escape as ''. Optional single semicolon. INSERT upserts. No binary NUL
   * literals. SELECT appends raw value to result (required only for SELECT).
   * 0 success, 1 missing SELECT/DELETE, -1 syntax/invalid, other negative
   * storage/OOM. error_offset receives first unparsed byte for syntax,
   * SIZE_MAX otherwise. */
  int mulib_sql_execute (mulib_memtable_t *table, const char *sql,
                         size_t length, mulib_buffer_t *result,
                         size_t *error_offset);
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
| Header | [`include/mulib-sql.h`](../include/mulib-sql.h) |
| Implementation switch | `MULIB_IMPL_SQL` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-memtable](mulib-memtable.md), [mulib-strbuf](mulib-strbuf.md) |
| Complete example | [`examples/modules/sql.c`](../examples/modules/sql.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/sql.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
