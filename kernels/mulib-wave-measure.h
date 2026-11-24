/** @file mulib-wave-measure.h
 * @brief Sample-weighted RMS, extrema, threshold crossing counts and time-domain integration.
 * @details C11 single-header implementation; define MULIB_IMPL_WAVE_MEASURE in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_WAVE_MEASURE_H
#define MULIB_WAVE_MEASURE_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-wave-signal.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_wave_measure_rms(const mulib_wave_signal_t *s, double *out);
int mulib_wave_measure_peak(const mulib_wave_signal_t *s, double *lo, double *hi);
int mulib_wave_measure_crossings(const mulib_wave_signal_t *s, double threshold, size_t *out);
int mulib_wave_measure_integral(const mulib_wave_signal_t *s, double *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_WAVE_MEASURE_H */
#if defined(MULIB_IMPL_WAVE_MEASURE) && !defined(MULIB_WAVE_MEASURE_H_IMPLEMENTED)
#define MULIB_WAVE_MEASURE_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_wave_measure_rms(const mulib_wave_signal_t *s, double *out) {
    if (!mulib_wave_signal_valid(s) || !out || !s->count)
        return 0;
    double scale = 0, ss = 0;
    for (size_t i = 0; i < s->count; i++) {
        double v = fabs(s->samples[i].value);
        if (v > scale) {
            ss = scale ? ss * (scale / v) * (scale / v) : 0;
            scale = v;
        }
        if (scale)
            ss += (v / scale) * (v / scale);
    }
    *out = scale ? sqrt(ss / s->count) * scale : 0;
    return 1;
}
int mulib_wave_measure_peak(const mulib_wave_signal_t *s, double *lo, double *hi) {
    if (!mulib_wave_signal_valid(s) || !lo || !hi || !s->count)
        return 0;
    double a = s->samples[0].value, b = a;
    for (size_t i = 1; i < s->count; i++) {
        double v = s->samples[i].value;
        if (v < a)
            a = v;
        if (v > b)
            b = v;
    }
    *lo = a;
    *hi = b;
    return 1;
}
int mulib_wave_measure_crossings(const mulib_wave_signal_t *s, double t, size_t *out) {
    if (!mulib_wave_signal_valid(s) || !out || !isfinite(t))
        return 0;
    size_t n = 0;
    for (size_t i = 1; i < s->count; i++) {
        double a = s->samples[i - 1].value - t, b = s->samples[i].value - t;
        if ((a < 0 && b >= 0) || (a > 0 && b <= 0))
            n++;
    }
    *out = n;
    return 1;
}
int mulib_wave_measure_integral(const mulib_wave_signal_t *s, double *out) {
    if (!out || !mulib_wave_signal_valid(s) || !s->count)
        return 0;
    double sum = 0, correction = 0;
    for (size_t i = 1; i < s->count; i++) {
        double dt = s->samples[i].time - s->samples[i - 1].time;
        double v = s->kind == MULIB_WAVE_DIGITAL
                       ? s->samples[i - 1].value
                       : .5 * s->samples[i - 1].value + .5 * s->samples[i].value;
        double term = dt * v - correction, next = sum + term;
        correction = (next - sum) - term;
        sum = next;
        if (!isfinite(sum))
            return 0;
    }
    *out = sum;
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_WAVE_MEASURE */
