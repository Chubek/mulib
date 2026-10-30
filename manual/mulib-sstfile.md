# sstfile

Versioned MuLIB sorted-table snapshots, not LevelDB/RocksDB files. `encode`
appends a complete table atomically. `decode` creates an owned memtable in an
uninitialized destination; outputs unchanged on failure. Returns 0 success,
negative invalid/corrupt/allocation errors. Empty tables are supported.

Wire: eight-byte `MuSST` version-1 magic, LE64 count, then each record's original
LE64 sequence and a WAL frame (ordinal sequence 1..count), then whole-file CRC32.
Decoded keys must be strictly increasing. Duplicate/unsorted keys, bad checksums,
truncation and trailing bytes fail. Values, tombstones and sequences roundtrip.
Decoded tables own their bytes; free with `mulib_memtable_free`.

Switches: `MULIB_IMPL_SSTFILE`, `MULIB_IMPL_MEMTABLE`, `MULIB_IMPL_WAL`,
`MULIB_IMPL_BUFFER`, `MULIB_IMPL_BYCUTILS`, `MULIB_IMPL_CRC32`.
Example: `examples/modules/sstfile.c`.
