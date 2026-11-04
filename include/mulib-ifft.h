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
#if defined(MULIB_IMPL_IFFT) && !defined(MULIB_IFFT_IMPLEMENTED)
#define MULIB_IFFT_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ifft(mulib_complex_t *a, size_t n) { return mulib_fft_transform(a, n, 1); }
#ifdef __cplusplus
}
#endif
#endif
