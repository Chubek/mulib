# Radix-2 FFT

[Manuals](README.md) · [Library index](index.md)

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

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-fft.h`](../include/mulib-fft.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-fft.h
 * @brief In-place radix-2 complex Fourier transforms.
 * Define MULIB_IMPL_FFT; mulib-complex supplies the public value type. Link libm.
 */
#ifndef MULIB_FFT_H
#define MULIB_FFT_H
#include "mulib-complex.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Forward DFT, exp(-2*pi*i*k*j/n), no normalization. n must be a positive
 * power of two. Invalid/nonfinite input returns 0 before mutation. */
int mulib_fft(mulib_complex_t *values, size_t n);
/** Same transform with direction; inverse!=0 uses positive phase and divides by n. */
int mulib_fft_transform(mulib_complex_t *values, size_t n, int inverse);
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
| Header | [`include/mulib-fft.h`](../include/mulib-fft.h) |
| Implementation switch | `MULIB_IMPL_FFT` |
| Repository-local includes | [mulib-complex](mulib-complex.md) |
| Complete example | [`examples/modules/fft.c`](../examples/modules/fft.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/fft.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
