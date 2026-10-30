# blockidx

Sparse binary fence-key to uint64 block-offset map. Zero initialize. `add` copies
or replaces the minimum key associated with a block. `find` returns the offset
for the greatest fence <= search key: 0 found, 1 below all fences, negative error.
It does not check block contents or impose increasing offsets. Empty keys work.

Uses memtable ordering; offsets are encoded explicitly rather than native-cast.
Free/reset with `mulib_blockidx_free`.
Switches: `MULIB_IMPL_BLOCKIDX`, `MULIB_IMPL_MEMTABLE`, `MULIB_IMPL_BYCUTILS`.
Example: `examples/modules/blockidx.c`.
