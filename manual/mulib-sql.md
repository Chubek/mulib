# sql

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
