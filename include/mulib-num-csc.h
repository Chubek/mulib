/** @file mulib-num-csc.h
 * @brief Compressed sparse column conversion, validation and matvec.
 * @details C11 single-header implementation; define MULIB_IMPL_NUM_CSC in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_NUM_CSC_H
#define MULIB_NUM_CSC_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-num-sparse.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_num_csc {
    size_t rows, cols, nnz;
    size_t *offset, *row;
    double *values;
} mulib_num_csc_t;
int mulib_num_csc_from_triplets(const mulib_num_sparse_t *s, mulib_num_csc_t *out);
int mulib_num_csc_matvec(const mulib_num_csc_t *s, const double *x, double *y);
void mulib_num_csc_free(mulib_num_csc_t *s);
int mulib_num_csc_validate(const mulib_num_csc_t *s);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_NUM_CSC_H */
#if defined(MULIB_IMPL_NUM_CSC) && !defined(MULIB_NUM_CSC_H_IMPLEMENTED)
#define MULIB_NUM_CSC_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
void mulib_num_csc_free(mulib_num_csc_t *s) {
    if (!s)
        return;
    free(s->offset);
    free(s->row);
    free(s->values);
    memset(s, 0, sizeof *s);
}
int mulib_num_csc_from_triplets(const mulib_num_sparse_t *s, mulib_num_csc_t *out) {
    if (!mulib_num_sparse_validate(s) || !out || s->count > SIZE_MAX / sizeof(double) ||
        s->cols == SIZE_MAX || s->count > SIZE_MAX / sizeof(size_t) ||
        s->cols > SIZE_MAX / sizeof(size_t) - 1)
        return 0;
    mulib_num_csc_t z = {0};
    z.rows = s->rows;
    z.cols = s->cols;
    z.nnz = s->count;
    z.offset = calloc(s->cols + 1, sizeof(size_t));
    if (!z.offset)
        return 0;
    if (s->count) {
        z.row = malloc(s->count * sizeof(size_t));
        z.values = malloc(s->count * sizeof(double));
        if (!z.row || !z.values) {
            mulib_num_csc_free(&z);
            return 0;
        }
    }
    for (size_t i = 0; i < s->count; i++)
        z.offset[s->triplets[i].col + 1]++;
    for (size_t i = 1; i <= z.cols; i++)
        z.offset[i] += z.offset[i - 1];
    size_t *cur = malloc(z.cols * sizeof(size_t));
    if (z.cols && !cur) {
        mulib_num_csc_free(&z);
        return 0;
    }
    if (z.cols)
        memcpy(cur, z.offset, z.cols * sizeof(size_t));
    for (size_t i = 0; i < s->count; i++) {
        size_t k = cur[s->triplets[i].col]++;
        z.row[k] = s->triplets[i].row;
        z.values[k] = s->triplets[i].value;
    }
    free(cur);
    for (size_t j = 0; j < z.cols; j++)
        for (size_t k = z.offset[j] + (z.offset[j] < z.offset[j + 1]); k < z.offset[j + 1]; k++) {
            size_t idx = z.row[k], q = k;
            double v = z.values[k];
            while (q > z.offset[j] && z.row[q - 1] > idx) {
                z.row[q] = z.row[q - 1];
                z.values[q] = z.values[q - 1];
                q--;
            }
            z.row[q] = idx;
            z.values[q] = v;
        }
    *out = z;
    return 1;
}
int mulib_num_csc_matvec(const mulib_num_csc_t *s, const double *x, double *y) {
    if (!mulib_num_csc_validate(s) || !x || !y || s->rows > SIZE_MAX / sizeof(double))
        return 0;
    double *r = calloc(s->rows ? s->rows : 1, sizeof(double));
    if (!r)
        return 0;
    int ok = 1;
    for (size_t j = 0; j < s->cols && ok; j++)
        for (size_t k = s->offset[j]; k < s->offset[j + 1]; k++) {
            size_t row = s->row[k];
            r[row] += s->values[k] * x[j];
            if (!isfinite(r[row])) {
                ok = 0;
                break;
            }
        }
    if (ok && s->rows)
        memcpy(y, r, s->rows * sizeof(double));
    free(r);
    return ok;
}
int mulib_num_csc_validate(const mulib_num_csc_t *s) {
    if (!s || !s->offset || s->cols == SIZE_MAX || (s->nnz && (!s->row || !s->values)) ||
        s->offset[0] != 0 || s->offset[s->cols] != s->nnz)
        return 0;
    for (size_t j = 0; j < s->cols; j++)
        if (s->offset[j] > s->offset[j + 1] || s->offset[j + 1] > s->nnz)
            return 0;
    for (size_t k = 0; k < s->nnz; k++)
        if (s->row[k] >= s->rows || !isfinite(s->values[k]))
            return 0;
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_NUM_CSC */
