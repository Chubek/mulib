# Row table

Define `MULIB_IMPL_ROW_RDBMS`, `DOCUMENTDB`, `DBSCHEMA` and their implementation
closures. Set the borrowed schema before use. `put/get/delete` use uint64 ids,
owned JSON records, memtable sequences and tombstones; zero means write success.
No SQL or persistence. [Example](../examples/modules/row-rdbms.c),
[contracts](storage-server.md#storage-and-serialization).
