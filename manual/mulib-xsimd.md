# mulib-xsimd

Original C11 float-array kernels with runtime scalar/SSE4.1/AVX2/NEON dispatch.
Define `MULIB_IMPL_XSIMD` in exactly one translation unit. No link dependencies.
See `examples/modules/xsimd.c`.

`mulib_xsimd_available` checks both compiled support and the running CPU.
`AUTO` selects AVX2, SSE4.1, AArch64 NEON, then scalar, in that order. An explicit
unavailable backend fails without touching output. On GCC/Clang x86, target
attributes isolate ISA-specific code: the application does not need `-mavx2` or
`-msse4.1`. Compiler CPU detection includes operating-system AVX state support.
AArch64 builds use baseline Advanced SIMD when `__ARM_NEON` is defined. Other
architectures/compilers, including the current MSVC implementation, use scalar.
Do not globally enable a newer ISA if the executable must run on older CPUs.

`scale(dst,src,n,gain)` overwrites, `mix` accumulates, and `dot` reduces into a
float. All return nonzero on success. Arrays may be unaligned; exact input/output
alias is supported, partial overlap is not. NULL arrays are allowed only for
zero length. No allocation, global mutable dispatch state, or synchronization is
required. The caller supplies valid arrays of at least `n` floats. Arithmetic
uses IEEE-like C float semantics; overflow/NaN are not converted to errors.
Reduction ordering and rounding can vary by backend. Kernels do not use FMA.

Codec `mulib_audio_options_t.simd` accepts these same backends. WAV and FLAC use
vector normalization/quantization scaling, MP3 and Vorbis use vector transform
dot products, and Audrey uses vector gain/mixing. This is targeted acceleration,
not a claim that bitstream parsing and every codec stage are vectorized.
