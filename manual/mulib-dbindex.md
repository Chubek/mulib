# dbindex

Binary key to uint64 row mapping over shared memtable records. Zero initialize;
`set` copies/replaces, `get` decodes a row ID, `remove` writes a tombstone.
Empty/NUL-containing keys are supported. Returns 0 success, 1 absent/deleted,
negative invalid/allocation errors; lookup output unchanged on failure.

Does not automatically update a row store or other indexes. `free` releases all
keys/tombstones. Switches: `MULIB_IMPL_DBINDEX`, `MULIB_IMPL_MEMTABLE`,
`MULIB_IMPL_BYCUTILS`. Example: `examples/modules/dbindex.c`.
