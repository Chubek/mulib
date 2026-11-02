# Graph records

Define `MULIB_IMPL_GRAPHDB` and `MEMTABLE`. Directed uint64 source/label/target
triples are ordered and sequence-aware; deletion writes tombstones. Neighbor
callbacks borrow records and must not mutate the store. Zero callback stops.
[Example](../examples/modules/graphdb.c), [contracts](storage-server.md).
