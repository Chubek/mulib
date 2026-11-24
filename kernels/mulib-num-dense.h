/** @file mulib-num-dense.h
 * @brief Dense double matrices and Gaussian elimination with partial row pivoting.
 * @details C11 single-header implementation; define MULIB_IMPL_NUM_DENSE in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_NUM_DENSE_H
#define MULIB_NUM_DENSE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_num_dense {
    size_t rows, cols;
    double *a;
} mulib_num_dense_t;
int mulib_num_dense_init(mulib_num_dense_t *m, size_t rows, size_t cols);
void mulib_num_dense_free(mulib_num_dense_t *m);
int mulib_num_dense_solve(size_t n, const double *a, const double *b, double *solution, double eps);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_NUM_DENSE_H */
#if defined(MULIB_IMPL_NUM_DENSE) && !defined(MULIB_NUM_DENSE_H_IMPLEMENTED)
#define MULIB_NUM_DENSE_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_dense_init(mulib_num_dense_t *m, size_t r, size_t c) {
    if (!m || (r && r > SIZE_MAX / sizeof(double)) || (r && c > SIZE_MAX / (r * sizeof(double))))
        return 0;
    memset(m, 0, sizeof *m);
    if (r && c) {
        m->a = calloc(r * c, sizeof(double));
        if (!m->a)
            return 0;
    }
    m->rows = r;
    m->cols = c;
    return 1;
}
void mulib_num_dense_free(mulib_num_dense_t *m) {
    if (!m)
        return;
    free(m->a);
    memset(m, 0, sizeof *m);
}
int mulib_num_dense_solve(size_t n, const double *a, const double *b, double *sol, double eps) {
    if (!n || n > SIZE_MAX / n || n * n > SIZE_MAX / sizeof(double) || !a || !b || !sol ||
        eps <= 0 || !isfinite(eps))
        return 0;
    for (size_t i = 0; i < n * n; i++)
        if (!isfinite(a[i]))
            return 0;
    for (size_t i = 0; i < n; i++)
        if (!isfinite(b[i]))
            return 0;
    double *m = malloc(n * n * sizeof(double)), *rhs = malloc(n * sizeof(double));
    if (!m || !rhs) {
        free(m);
        free(rhs);
        return 0;
    }
    memcpy(m, a, n * n * sizeof(double));
    memcpy(rhs, b, n * sizeof(double));
    int ok = 1;
    for (size_t k = 0; k < n && ok; k++) {
        size_t p = k;
        double max = fabs(m[k * n + k]);
        for (size_t i = k + 1; i < n; i++)
            if (fabs(m[i * n + k]) > max) {
                max = fabs(m[i * n + k]);
                p = i;
            }
        if (!isfinite(max) || max <= eps) {
            ok = 0;
            break;
        }
        if (p != k) {
            for (size_t j = k; j < n; j++) {
                double t = m[k * n + j];
                m[k * n + j] = m[p * n + j];
                m[p * n + j] = t;
            }
            double t = rhs[k];
            rhs[k] = rhs[p];
            rhs[p] = t;
        }
        for (size_t i = k + 1; i < n; i++) {
            double f = m[i * n + k] / m[k * n + k];
            m[i * n + k] = 0;
            for (size_t j = k + 1; j < n; j++)
                m[i * n + j] -= f * m[k * n + j];
            rhs[i] -= f * rhs[k];
        }
    }
    if (ok) {
        for (size_t i = n; i-- > 0;) {
            double sum = rhs[i];
            for (size_t j = i + 1; j < n; j++)
                sum -= m[i * n + j] * rhs[j];
            rhs[i] = sum / m[i * n + i];
            if (!isfinite(rhs[i])) {
                ok = 0;
                break;
            }
        }
    }
    if (ok) {
        memcpy(sol, rhs, n * sizeof *sol);
    }
    free(rhs);
    free(m);
    return ok;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_NUM_DENSE */
