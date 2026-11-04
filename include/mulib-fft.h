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
#if defined(MULIB_IMPL_FFT) && !defined(MULIB_FFT_IMPLEMENTED)
#define MULIB_FFT_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fft_transform(mulib_complex_t *a, size_t n, int inverse)
{
  if (!a || !n || (n & (n - 1)) || n > SIZE_MAX / sizeof(*a)) return 0;
  for (size_t i = 0; i < n; ++i) if (!isfinite(a[i].real) || !isfinite(a[i].imag)) return 0;
  for (size_t i = 1, j = 0; i < n; ++i) {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) { mulib_complex_t t = a[i]; a[i] = a[j]; a[j] = t; }
  }
  for (size_t length = 2; length <= n;) {
    double phase = (inverse ? 2 : -2) * 3.14159265358979323846 / (double)length;
    for (size_t base = 0; base < n; base += length) {
      for (size_t j = 0; j < length / 2; ++j) {
        double c = cos(phase * j), s = sin(phase * j);
        mulib_complex_t u = a[base + j], v = a[base + j + length / 2];
        double re = v.real * c - v.imag * s, im = v.real * s + v.imag * c;
        a[base + j].real = u.real + re; a[base + j].imag = u.imag + im;
        a[base + j + length / 2].real = u.real - re;
        a[base + j + length / 2].imag = u.imag - im;
      }
    }
    if (length == n) break;
    length *= 2;
  }
  if (inverse) for (size_t i = 0; i < n; ++i) { a[i].real /= n; a[i].imag /= n; }
  return 1;
}
int mulib_fft(mulib_complex_t *a, size_t n) { return mulib_fft_transform(a, n, 0); }
#ifdef __cplusplus
}
#endif
#endif
