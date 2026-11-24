/** @file mulib-wave-signal.h
 * @brief Owned finite time-value sample series with nondecreasing timestamps, validation and
 * interpolation.
 * @details C11 single-header implementation; define MULIB_IMPL_WAVE_SIGNAL in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_WAVE_SIGNAL_H
#define MULIB_WAVE_SIGNAL_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_wave_kind { MULIB_WAVE_DIGITAL = 1, MULIB_WAVE_ANALOG = 2 } mulib_wave_kind_t;
typedef struct mulib_wave_sample {
    double time, value;
} mulib_wave_sample_t;
typedef struct mulib_wave_signal {
    mulib_wave_sample_t *samples;
    size_t count, cap;
    mulib_wave_kind_t kind;
} mulib_wave_signal_t;
int mulib_wave_signal_push(mulib_wave_signal_t *s, double time, double value);
void mulib_wave_signal_free(mulib_wave_signal_t *s);
int mulib_wave_signal_valid(const mulib_wave_signal_t *s);
int mulib_wave_signal_interpolate(const mulib_wave_signal_t *s, double time, double *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_WAVE_SIGNAL_H */
#if defined(MULIB_IMPL_WAVE_SIGNAL) && !defined(MULIB_WAVE_SIGNAL_H_IMPLEMENTED)
#define MULIB_WAVE_SIGNAL_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_wave_signal_push(mulib_wave_signal_t *s, double t, double v) {
    if (!s || s->count > s->cap || (s->count && !s->samples) || !isfinite(t) || !isfinite(v) ||
        (s->count && t < s->samples[s->count - 1].time))
        return 0;
    if (s->count == s->cap) {
        size_t n = s->cap ? s->cap * 2 : 32;
        if (n <= s->cap || n > SIZE_MAX / sizeof *s->samples)
            return 0;
        void *p = realloc(s->samples, n * sizeof *s->samples);
        if (!p)
            return 0;
        s->samples = p;
        s->cap = n;
    }
    s->samples[s->count++] = (mulib_wave_sample_t){t, v};
    return 1;
}
void mulib_wave_signal_free(mulib_wave_signal_t *s) {
    if (!s)
        return;
    free(s->samples);
    memset(s, 0, sizeof *s);
}
int mulib_wave_signal_valid(const mulib_wave_signal_t *s) {
    if (!s || s->count > s->cap || (s->count && !s->samples))
        return 0;
    for (size_t i = 0; i < s->count; i++)
        if (!isfinite(s->samples[i].time) || !isfinite(s->samples[i].value) ||
            (i && s->samples[i].time < s->samples[i - 1].time))
            return 0;
    return 1;
}
int mulib_wave_signal_interpolate(const mulib_wave_signal_t *s, double t, double *out) {
    if (!out || !isfinite(t) || !mulib_wave_signal_valid(s) || !s->count ||
        t < s->samples[0].time || t > s->samples[s->count - 1].time)
        return 0;
    size_t lo = 0, hi = s->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (s->samples[mid].time <= t)
            lo = mid + 1;
        else
            hi = mid;
    }
    size_t i = lo - 1;
    double v = s->samples[i].value;
    /* Repeated timestamps use the last sample. Digital values are held. */
    if (s->kind != MULIB_WAVE_DIGITAL && lo < s->count && t != s->samples[i].time) {
        double w = (t - s->samples[i].time) / (s->samples[lo].time - s->samples[i].time);
        v = (1 - w) * v + w * s->samples[lo].value;
    }
    if (!isfinite(v))
        return 0;
    *out = v;
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_WAVE_SIGNAL */
