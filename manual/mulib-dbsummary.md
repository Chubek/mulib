# dbsummary

`compute` calculates live/deleted record counts, stored key/value byte totals and
maximum sequence for a memtable. Includes tombstone keys in byte totals. Empty
tables produce zeros. It checks size_t addition overflow before publishing any
output. Returns 1 success, 0 invalid/overflow. It does not count allocator overhead,
SST bytes, cache bytes or estimate disk space.

Switches: `MULIB_IMPL_DBSUMMARY`; `MULIB_IMPL_MEMTABLE` when using table operations.
Example: `examples/modules/dbsummary.c`.
