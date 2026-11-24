/** @file mulib-num-statistics.h
 * @brief Streaming Welford mean, sample variance and extrema, with checked count growth and merging
 * of independent summaries.
 * @details C11 single-header implementation; define MULIB_IMPL_NUM_STATISTICS in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_NUM_STATISTICS_H
#define MULIB_NUM_STATISTICS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_num_stat {
    size_t n;
    double mean, m2, min, max;
} mulib_num_stat_t;
int mulib_num_stat_push(mulib_num_stat_t *s, double sample);
int mulib_num_stat_variance(const mulib_num_stat_t *s, double *out);
int mulib_num_stat_merge(mulib_num_stat_t *s, const mulib_num_stat_t *other);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_NUM_STATISTICS_H */
#if defined(MULIB_IMPL_NUM_STATISTICS) && !defined(MULIB_NUM_STATISTICS_H_IMPLEMENTED)
#define MULIB_NUM_STATISTICS_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_stat_push(mulib_num_stat_t *s, double x) {
    if (!s || !isfinite(x) || s->n == SIZE_MAX)
        return 0;
    if (!s->n) {
        s->n = 1;
        s->mean = s->min = s->max = x;
        s->m2 = 0;
        return 1;
    }
    size_t n = s->n + 1;
    double delta = x - s->mean, mean = s->mean + delta / n, m2 = s->m2 + delta * (x - mean);
    if (!isfinite(mean) || !isfinite(m2))
        return 0;
    if (!s->n) {
        s->min = x;
        s->max = x;
    } else {
        if (x < s->min)
            s->min = x;
        if (x > s->max)
            s->max = x;
    }
    s->n = n;
    s->mean = mean;
    s->m2 = m2;
    return 1;
}
int mulib_num_stat_variance(const mulib_num_stat_t *s, double *out) {
    if (!s || !out || s->n < 2 || !isfinite(s->m2) || s->m2 < 0)
        return 0;
    *out = s->m2 / (s->n - 1);
    return 1;
}
int mulib_num_stat_merge(mulib_num_stat_t *s, const mulib_num_stat_t *other) {
    if (!s || !other || other->n > SIZE_MAX - s->n)
        return 0;
    if (!other->n)
        return 1;
    if (!isfinite(other->mean) || !isfinite(other->m2) || other->m2 < 0 || !isfinite(other->min) ||
        !isfinite(other->max) || other->min > other->max)
        return 0;
    if (!s->n) {
        *s = *other;
        return 1;
    }
    if (!isfinite(s->mean) || !isfinite(s->m2) || s->m2 < 0)
        return 0;
    mulib_num_stat_t r = *s;
    size_t n = s->n + other->n;
    double d = other->mean - s->mean, w = (double)other->n / n;
    r.mean = s->mean + d * w;
    r.m2 = s->m2 + other->m2 + d * d * ((double)s->n * w);
    if (!isfinite(r.mean) || !isfinite(r.m2))
        return 0;
    r.n = n;
    r.min = fmin(s->min, other->min);
    r.max = fmax(s->max, other->max);
    *s = r;
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_NUM_STATISTICS */
