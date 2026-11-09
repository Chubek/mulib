/** @file mulib-spice-measure.h
 * @brief SPICE-facing sample-domain RMS and extrema helpers delegated to the existing wave-measure
 * primitive.
 * @details C11 single-header implementation; define MULIB_IMPL_SPICE_MEASURE in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SPICE_MEASURE_H
#define MULIB_SPICE_MEASURE_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-wave-measure.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_measure_rms(const mulib_wave_signal_t *samples, double *out);
int mulib_spice_measure_peaks(const mulib_wave_signal_t *samples, double *minimum, double *maximum);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SPICE_MEASURE_H */
#if defined(MULIB_IMPL_SPICE_MEASURE) && !defined(MULIB_SPICE_MEASURE_H_IMPLEMENTED)
#define MULIB_SPICE_MEASURE_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_measure_rms(const mulib_wave_signal_t *s, double *out) {
    return mulib_wave_measure_rms(s, out);
}
int mulib_spice_measure_peaks(const mulib_wave_signal_t *s, double *lo, double *hi) {
    return mulib_wave_measure_peak(s, lo, hi);
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_SPICE_MEASURE */
