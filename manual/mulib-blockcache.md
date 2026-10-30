# blockcache

Initialize with nonzero entry/byte limits. `put` copies a uint64-keyed block;
oversized/invalid/allocation failures return 0 without eviction. On success it
replaces the existing key and evicts least-recently used blocks until both limits
hold. Inputs may alias cached bytes. Zero-length blocks still count as entries.

`get` promotes an entry and borrows bytes until the next mutation/free. Returns
1 hit, 0 absent/invalid; outputs unchanged on miss. No pinning, locking, file I/O
or thread safety. Release/reset with `free`. Lookup/promotion is linear.
Switch: `MULIB_IMPL_BLOCKCACHE`. Example: `examples/modules/blockcache.c`.
