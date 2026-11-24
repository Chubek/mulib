/** @file mulib-wave-decimate.h
 * @brief Min/max display decimation in index-aligned buckets, preserving extrema.
 * @details C11 single-header implementation; define MULIB_IMPL_WAVE_DECIMATE in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_WAVE_DECIMATE_H
#define MULIB_WAVE_DECIMATE_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-wave-signal.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_wave_decimate_minmax(const mulib_wave_signal_t *s, size_t buckets,
                               mulib_wave_sample_t *minima, mulib_wave_sample_t *maxima);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_WAVE_DECIMATE_H */
#if defined(MULIB_IMPL_WAVE_DECIMATE) && !defined(MULIB_WAVE_DECIMATE_H_IMPLEMENTED)
#define MULIB_WAVE_DECIMATE_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_wave_decimate_minmax(const mulib_wave_signal_t *s, size_t k, mulib_wave_sample_t *lo,
                               mulib_wave_sample_t *hi) {
    if (!mulib_wave_signal_valid(s) || !k || !lo || !hi || s->count < k)
        return 0;
    size_t start = 0, rem = 0, base = s->count / k, extra = s->count % k;
    for (size_t j = 0; j < k; j++) {
        size_t width = base;
        if (rem >= k - extra) {
            width++;
            rem -= k - extra;
        } else
            rem += extra;
        size_t end = start + width;
        if (end <= start)
            return 0;
        mulib_wave_sample_t a = s->samples[start], b = a;
        for (size_t i = start + 1; i < end; i++) {
            mulib_wave_sample_t v = s->samples[i];
            if (v.value < a.value)
                a = v;
            if (v.value > b.value)
                b = v;
        }
        lo[j] = a;
        hi[j] = b;
        start = end;
    }
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_WAVE_DECIMATE */
