/** @file mulib-distfield.h
 * @brief Linear-time Euclidean signed-distance fields for scalable glyph masks.
 * Define MULIB_IMPL_DISTFIELD once; link libm on Unix. No font/runtime dependency.
 */
#ifndef MULIB_DISTFIELD_H
#define MULIB_DISTFIELD_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Build an owned, tightly packed 8-bit SDF from a borrowed alpha mask.
 * Alpha >=128 is inside. Pixel-center Euclidean distances minus half a pixel
 * are positive inside, negative outside. Off-image pixels count as outside.
 * Encode round(255*clamp(.5 + distance/(2*spread),0,1)); spread finite and >0.
 * 0/-1, output unchanged on failure. Empty dimensions succeed with NULL.
 * Dimensions <=INT_MAX; checked allocations. O(width*height) time/storage.
 * Add transparent padding before generation for an exterior glyph halo. */
int mulib_distfield_generate(const unsigned char *mask, size_t width, size_t height,
                             size_t stride, double spread, unsigned char **field);
/** @brief Decode a field sample to signed distance using the original spread. */
double mulib_distfield_distance(unsigned char sample, double spread);
/** @brief Smooth coverage at a sampled field value (0..1); edge_width is the
 * positive transition width in normalized SDF units. Invalid args return 0. */
double mulib_distfield_coverage(double sample, double edge_width);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DISTFIELD) && !defined(MULIB_DISTFIELD_IMPLEMENTED)
#define MULIB_DISTFIELD_IMPLEMENTED
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Lower envelope of squared-distance parabolas. */
static void mulib_distfield_line_(const double *f, double *d, size_t n, size_t *v, double *z) {
    size_t q, k = 0;
    double s;
    v[0] = 0; z[0] = -1e30; z[1] = 1e30;
    for (q = 1; q < n; ++q) {
        for (;;) {
            double a = (double)q, b = (double)v[k];
            s = ((f[q] + a*a) - (f[v[k]] + b*b)) / (2*(a-b));
            if (!k || s > z[k]) break;
            --k;
        }
        ++k; v[k] = q; z[k] = s; z[k+1] = 1e30;
    }
    k = 0;
    for (q = 0; q < n; ++q) {
        double delta;
        while (z[k+1] < (double)q) ++k;
        delta = (double)q - (double)v[k]; d[q] = delta*delta + f[v[k]];
    }
}
static void mulib_distfield_transform_(double *d, size_t w, size_t h, double *f, double *g, size_t *v, double *z) {
    size_t x, y;
    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) f[x] = d[y*w+x];
        mulib_distfield_line_(f, g, w, v, z);
        for (x = 0; x < w; ++x) d[y*w+x] = g[x];
    }
    for (x = 0; x < w; ++x) {
        for (y = 0; y < h; ++y) f[y] = d[y*w+x];
        mulib_distfield_line_(f, g, h, v, z);
        for (y = 0; y < h; ++y) d[y*w+x] = g[y];
    }
}
int mulib_distfield_generate(const unsigned char *mask, size_t w, size_t h, size_t stride, double spread, unsigned char **out) {
    double *d = NULL, *f = NULL, *g = NULL, *z = NULL;
    size_t *v = NULL, n, m, x, y, i;
    unsigned char *result = NULL;
    int pass, has_inside = 0, status = -1;
    if (!out || !isfinite(spread) || spread <= 0 || w > INT_MAX || h > INT_MAX || stride < w) return -1;
    if (!w || !h) { *out = NULL; return 0; }
    if (!mask || w > SIZE_MAX / h || (h - 1) > (SIZE_MAX - w) / stride) return -1;
    n = w*h; m = w > h ? w : h;
    if (n > SIZE_MAX / sizeof *d || m > SIZE_MAX / sizeof *z - 1 || m > SIZE_MAX / sizeof *v) return -1;
    for (y = 0; y < h && !has_inside; ++y)
        for (x = 0; x < w; ++x) if (mask[y*stride+x] >= 128) { has_inside = 1; break; }
    if (!has_inside) {
        result = (unsigned char *)calloc(n, 1);
        if (!result) return -1;
        *out = result; return 0;
    }
    d = (double *)malloc(n * sizeof *d); f = (double *)malloc(m * sizeof *f);
    g = (double *)malloc(m * sizeof *g); z = (double *)malloc((m + 1) * sizeof *z);
    v = (size_t *)malloc(m * sizeof *v); result = (unsigned char *)malloc(n);
    if (!d || !f || !g || !z || !v || !result) goto done;
    for (pass = 0; pass < 2; ++pass) {
        for (y = 0; y < h; ++y) for (x = 0; x < w; ++x)
            d[y*w+x] = ((mask[y*stride+x] >= 128) == pass) ? 0 : 1e20;
        mulib_distfield_transform_(d, w, h, f, g, v, z);
        for (y = 0; y < h; ++y) for (x = 0; x < w; ++x) {
            double distance, encoded;
            if ((mask[y*stride+x] >= 128) == pass) continue;
            i = y*w+x;
            distance = sqrt(d[i]);
            if (!pass) {
                double border = (double)(x + 1);
                if ((double)(w-x) < border) border = (double)(w-x);
                if ((double)(y+1) < border) border = (double)(y+1);
                if ((double)(h-y) < border) border = (double)(h-y);
                if (border < distance) distance = border;
            }
            distance = (distance - 0.5) * (pass ? -1 : 1);
            encoded = 0.5 + (distance / spread) * 0.5;
            if (encoded < 0) encoded = 0;
            if (encoded > 1) encoded = 1;
            result[i] = (unsigned char)floor(encoded * 255 + 0.5);
        }
    }
    *out = result; result = NULL; status = 0;
done:
    free(d); free(f); free(g); free(z); free(v); free(result); return status;
}
double mulib_distfield_distance(unsigned char sample, double spread) {
    return isfinite(spread) && spread > 0 ? (sample / 255.0 - 0.5) * 2 * spread : 0;
}
double mulib_distfield_coverage(double sample, double width) {
    double t;
    if (!isfinite(sample) || !isfinite(width) || width <= 0) return 0;
    t = (sample - 0.5) / width + 0.5;
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    return t*t*(3-2*t);
}
#ifdef __cplusplus
}
#endif
#endif
