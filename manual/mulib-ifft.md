# Inverse FFT

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_IFFT` and `MULIB_IMPL_FFT`; link `-lm`.
`MULIB_IMPL_COMPLEX` is only needed for calls to complex arithmetic helpers.
[Example](../examples/modules/ifft.c).

`mulib_ifft` reuses the FFT engine with positive phase and division by n.
It inverts the forward transform within floating-point error. Caller owns the
in-place array of `mulib_complex_t`. Positive power-of-two lengths only;
invalid/nonfinite inputs return 0 before mutation. Finite arithmetic may
overflow. O(n log n) time, constant storage and no allocations.
See [FFT](mulib-fft.md) for sign conventions and supported scope.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ifft.h`](../include/mulib-ifft.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-ifft.h
 * @brief Normalized inverse radix-2 Fourier transform.
 * Define MULIB_IMPL_IFFT and MULIB_IMPL_FFT. Link libm.
 */
#ifndef MULIB_IFFT_H
#define MULIB_IFFT_H
#include "mulib-fft.h"
#ifdef __cplusplus
extern "C" {
#endif
/** In-place inverse DFT. Same validation and storage contract as mulib_fft. */
int mulib_ifft(mulib_complex_t *values, size_t n);
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
| Header | [`include/mulib-ifft.h`](../include/mulib-ifft.h) |
| Implementation switch | `MULIB_IMPL_IFFT` |
| Repository-local includes | [mulib-fft](mulib-fft.md) |
| Complete example | [`examples/modules/ifft.c`](../examples/modules/ifft.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ifft.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
