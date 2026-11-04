# Radix-2 FFT

Define `MULIB_IMPL_FFT`; link `-lm`. The `mulib-complex` dependency supplies
the public struct type; `MULIB_IMPL_COMPLEX` is needed only if using its arithmetic
functions. [Example](../examples/modules/fft.c).

Transforms a caller-owned array in place, forward sign exp(-2πijk/n), without
normalization. `mulib_fft_transform(...,inverse)` uses positive phase and 1/n
scaling when inverse is nonzero. n must be a positive power of two; n=1 works.
Missing arrays, invalid dimensions and nonfinite input return 0 before mutation.
Finite inputs can still overflow during arithmetic; numerical overflow is not
reported separately. O(n log n) time, constant storage, no allocations.
This is not an arbitrary-length, real-packed, multidimensional or windowed FFT.
