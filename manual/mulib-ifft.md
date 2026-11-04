# Inverse FFT

Define `MULIB_IMPL_IFFT` and `MULIB_IMPL_FFT`; link `-lm`.
`MULIB_IMPL_COMPLEX` is only needed for calls to complex arithmetic helpers.
[Example](../examples/modules/ifft.c).

`mulib_ifft` reuses the FFT engine with positive phase and division by n.
It inverts the forward transform within floating-point error. Caller owns the
in-place array of `mulib_complex_t`. Positive power-of-two lengths only;
invalid/nonfinite inputs return 0 before mutation. Finite arithmetic may
overflow. O(n log n) time, constant storage and no allocations.
See [FFT](mulib-fft.md) for sign conventions and supported scope.
