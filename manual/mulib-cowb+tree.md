# cowb+tree

Reference-counted snapshots over the existing integer B+ tree. Zero initialize.
`snapshot` shares state into a fresh destination. A write to a shared tree first
rebuilds a private tree; failed allocation leaves both views unchanged. This is
**whole-tree copy on first shared mutation**, not persistent page/path copying.

`set`/`get`/`remove` return 1 success/found and 0 invalid/missing/allocation failure.
Keys are int64 and values uint64. References are single-threaded, not atomic.
Free every snapshot with `free`; do not copy owning handles by assignment.

Switches: `MULIB_IMPL_COWBPTREE`, `MULIB_IMPL_BPTREE`.
Example: `examples/modules/cowb+tree.c`.
