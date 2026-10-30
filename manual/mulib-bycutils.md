# bycutils

Allocation-free binary-code arithmetic. `range` checks offset/length using
subtraction. `align` rounds size_t upward to a nonzero power of two, rejecting
overflow. `load`/`store` access unaligned little-endian integers of width 1..8.
Store rejects values that do not fit; all failures preserve outputs/bytes.
Functions return 1 success/valid, 0 invalid. Callers supply actual accessible
buffer sizes; no native structure casts or signed integer encoding.

Switch: `MULIB_IMPL_BYCUTILS`. Example: `examples/modules/bycutils.c`.
