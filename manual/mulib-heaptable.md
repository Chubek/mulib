# heaptable

Binary row storage with monotonically assigned positive uint64 row IDs. Zero
initialize. `insert` copies a row and returns its ID only on success. IDs are
never reused until `free` resets the table. `get` borrows row bytes until mutation
or release; `update` replaces or tombstones an existing live row. Deleted rows
return absent. Tombstones have empty values. No pages, disk or concurrent access.

Results: 0 success, 1 absent, negative invalid/allocation/overflow. Storage is the
shared sorted memtable with big-endian IDs as keys, not a physical disk heap.
Switches: `MULIB_IMPL_HEAPTABLE`, `MULIB_IMPL_MEMTABLE`.
Example: `examples/modules/heaptable.c`.
