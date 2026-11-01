# mulib-simdmath

SIMD graphics arithmetic sharing linalg's float/column-major conventions.
Define `MULIB_IMPL_SIMDMATH` and `MULIB_IMPL_LINALG` before inclusion; link `-lm`.

`mulib_simdmath_f4_t` contains four public float lanes. Public ABI and alignment
do not depend on the selected backend. `load` and `store` accept ordinary float
alignment; they access exactly four elements, so their pointers must be non-NULL
and reference at least four floats. They do not overread SIMD padding.

## Backends and operations

The implementation selects SSE2 on compiled SSE2 targets, otherwise NEON when
available, otherwise scalar C. Define `MULIB_SIMDMATH_SCALAR` in the implementation
translation unit to force the fallback. `backend()` reports `sse2`, `neon` or
`scalar`. There is no runtime CPU dispatch. AArch32 NEON uses scalar division
and square root; AArch64 uses native SIMD for those operations.

Broadcast, add/subtract/multiply/divide, square root and dot operate on four
lanes. Negative square roots and division by zero follow the platform's normal
floating-point behavior. Normalization scales by the largest absolute component
before computing length; zero/nonfinite input or NULL output returns 0 without
modifying output. Successful normalization returns 1.

`transform` multiplies a homogeneous vec4 by a linalg mat4; `mat4_mul` composes
matrices with SIMD column operations. `transform_many` handles any count,
including nonmultiples of four and zero. Exact in-place transformation is
supported; other overlap is a caller error. NULL arrays are allowed for zero
count. Invalid arrays/counts return 0, success returns 1. Ordinary float rounding
applies; results need not be bit-identical to linalg's double-accumulated matrix
multiplication. Do not enable flush-to-zero if subnormal preservation is required.

No allocations or mutable global state. `examples/modules/simdmath.c` prints the
active backend and transforms an array. `tests/test_graphics_math.c` and
`tests/test_simdmath_scalar.c` run numerical contracts through native and forced
scalar implementations.
