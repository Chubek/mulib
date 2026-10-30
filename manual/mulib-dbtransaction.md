# dbtransaction

Optimistic snapshot transaction over one shared memtable. Zero initialize both
objects; `begin` clones the target. Read/write `tx.working` using memtable APIs.
Target must outlive the active transaction. `commit` checks its captured revision,
then publishes working data without allocation. `abort` frees unpublished changes.

Results: 0 success, 1 commit conflict (working data retained), negative invalid,
allocation or revision overflow. A successful commit invalidates old target
borrows and makes the transaction inactive. No automatic retry, nested active
begin, multi-table atomicity, locking or disk durability. Direct structure-field
mutation bypasses revisions and is unsupported.
Switches: `MULIB_IMPL_DBTRANSACTION`, `MULIB_IMPL_MEMTABLE`.
Example: `examples/modules/dbtransaction.c`.
