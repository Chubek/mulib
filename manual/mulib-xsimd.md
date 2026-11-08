# mulib-xsimd

[Manuals](README.md) · [Library index](index.md)

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

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-xsimd.h`](../include/mulib-xsimd.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-xsimd.h
 * @brief Runtime-selected scalar, SSE4.1, AVX2 and AArch64 NEON float kernels.
 */
#ifndef MULIB_XSIMD_H
#define MULIB_XSIMD_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_xsimd_backend {
  MULIB_XSIMD_AUTO, MULIB_XSIMD_SCALAR, MULIB_XSIMD_SSE4,
  MULIB_XSIMD_AVX2, MULIB_XSIMD_NEON
} mulib_xsimd_backend_t;
/** Return whether this binary and host support a backend (including OS AVX state). */
int mulib_xsimd_available (mulib_xsimd_backend_t backend);
/** Resolve AUTO to the fastest available compiled backend. */
mulib_xsimd_backend_t mulib_xsimd_best (void);
/** dst[i]=src[i]*gain. Exact alias is allowed, partial overlap is not.
 * Unaligned arrays and arbitrary tails are supported. Return 0 for invalid
 * pointers/backend; output unchanged in that case. NULL is allowed for n=0. */
int mulib_xsimd_scale (mulib_xsimd_backend_t, float *dst, const float *src, size_t n, float gain);
/** dst[i]+=src[i]*gain; same alias and failure rules as scale. */
int mulib_xsimd_mix (mulib_xsimd_backend_t, float *dst, const float *src, size_t n, float gain);
/** Float dot product. Reduction rounding can differ between backends. */
int mulib_xsimd_dot (mulib_xsimd_backend_t, const float *a, const float *b, size_t n, float *out);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-xsimd.h`](../include/mulib-xsimd.h) |
| Implementation switch | `MULIB_IMPL_XSIMD` |
| Repository-local includes | None |
| Complete example | [`examples/modules/xsimd.c`](../examples/modules/xsimd.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/xsimd.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
