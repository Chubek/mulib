# lsmtree

Memory-backed LSM composition of mutable memtable, encoded immutable runs and WAL.
Zero initialize; `put` copies key/value or deletion and advances sequence. It
allocates candidate memory before appending WAL and publishing changes, so failed
writes leave logical state/WAL unchanged. `get` appends a copied value to a caller
buffer: 0 found, 1 missing/deleted, negative error. Clear its length to replace.

`flush` creates a run and clears mutable records; `compact` merges runs preserving
newest sequences/tombstones. Both are explicit and failure-atomic. Lookup decodes
runs into temporary tables; this reference implementation prioritizes checked
composition, not production read amplification or bounded disk storage.
`recover` constructs a fresh tree from a WAL and optionally accepts only a torn
final frame; consumed length reports the valid prefix. `free` discards everything.
No disk I/O, fsync, background threads or locking. WAL bytes are retained by flush.

Switches: `MULIB_IMPL_LSMTREE`, `MULIB_IMPL_SSTFILE`, `MULIB_IMPL_MEMTABLE`,
`MULIB_IMPL_WAL`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_BYCUTILS`, `MULIB_IMPL_CRC32`.
Example: `examples/modules/lsmtree.c`.
