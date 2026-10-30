# spatialtree

Immutable median-balanced 2D k-d tree. `build` copies finite x/y/uint64-ID points
into an uninitialized destination; duplicate positions/IDs are allowed. Sorting
per level costs O(n log² n). Empty construction succeeds. `free` resets storage.

`query` searches inclusive rectangles, pruning by split coordinate. Callback
borrows a point; return nonzero to continue or zero to stop. Count includes the
stopping callback; SIZE_MAX means invalid input. Callback must not mutate/free
the tree. No radius/nearest-neighbor API, dynamic insertion or disk index.
Switch: `MULIB_IMPL_SPATIALTREE`. Example: `examples/modules/spatialtree.c`.
