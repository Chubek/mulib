# Object records

Define `MULIB_IMPL_OBJECTDB` and `MEMTABLE`. Stores copied opaque bytes with
nonzero application type tags and binary keys, not live C pointers. Gets borrow
until mutation/free. Zero means success; type mismatch/absence is 1.
[Example](../examples/modules/objectdb.c), [contracts](storage-server.md).
