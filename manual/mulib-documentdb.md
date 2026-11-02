# Document records

Define `MULIB_IMPL_DOCUMENTDB`, `MEMTABLE`, `JSON`, `PARSEC`, `STRBUF` and `UTF8`.
Binary-keyed object documents are copied as JSON text. Gets return independent
owned values; free them with `mulib_json_destroy`. Writes use memtable result
codes, sequences and tombstones. No disk engine or query language.
[Example](../examples/modules/documentdb.c), [contracts](storage-server.md).
