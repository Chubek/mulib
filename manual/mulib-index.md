# index

Unique ordered int64-key to uint64-row-ID secondary index over the B+ tree. Zero
initialize. `insert` returns 0 for duplicates rather than replacing. `find` may
omit its value output, and `remove` returns whether a key existed. All successful
operations return 1; insert failures also include allocation/invalid input.

This index is independent of a row store: callers maintain cross-table
consistency. `free` releases nodes; no disk or concurrent synchronization.
Switches: `MULIB_IMPL_INDEX`, `MULIB_IMPL_BPTREE`.
Example: `examples/modules/index.c`.
