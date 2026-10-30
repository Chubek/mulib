# memtable

Owned binary keys/values in unsigned-byte lexicographic order. Initialize
`mulib_memtable_t` to zero. `put` copies inputs (including aliases into the table),
stores a uint64 sequence and a tombstone flag, and replaces only equal/newer
sequences. Tombstone values must be empty. Empty keys/values are valid.

`get` borrows a record, including deleted records; 0 found, 1 absent, negative
error. `lower_bound` returns the first key >= input. Inserts are O(n), lookups
O(log n). Mutation invalidates borrowed records. `clone` constructs a deep copy
in an uninitialized destination. `free` releases/reset storage. Failed writes
preserve logical state; revision increases only on successful mutation.

Switch: `MULIB_IMPL_MEMTABLE`. Example: `examples/modules/memtable.c`.
See [storage contracts and limits](extension-layers.md#database-storage).
