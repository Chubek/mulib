# Hamming distances

Define `MULIB_IMPL_HAMMING_DISTANCE`.
[Example](../examples/modules/hamming_distance.c).

Both functions borrow two buffers of the same supplied byte length.
`mulib_hamming_distance` counts differing bytes; `mulib_hamming_bits` counts
set bits in their XOR (all bits in each unsigned char). Embedded NUL is ordinary
data. Empty inputs return zero and may be NULL. Missing nonempty input or bit
count overflow returns SIZE_MAX. There is no implicit strlen or unequal-length
padding. O(length) time, no allocation or ownership transfer.
