# binfilereader

Initialize a cursor over borrowed bytes. `uint` reads unsigned little-endian
integers of width 1..8; `bytes` borrows/consumes a slice; `seek` permits absolute
positions through EOF. Return 1 success, 0 invalid/bounds, preserving cursor and
output on failure. Empty byte reads are allowed, integer reads require bytes.

Reader owns nothing and must not outlive input. No implicit endian guessing,
stream buffering, native casts or file I/O.
Switches: `MULIB_IMPL_BINFILEREADER`, `MULIB_IMPL_BINFILE`, `MULIB_IMPL_BYCUTILS`.
Example: `examples/modules/binfilereader.c`.
