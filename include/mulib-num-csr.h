/** @file mulib-num-csr.h
 * @brief Compressed sparse row conversion, validation, matvec and transpose.
 * @details C11 single-header implementation; define MULIB_IMPL_NUM_CSR in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_NUM_CSR_H
#define MULIB_NUM_CSR_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-num-sparse.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_num_csr {
    size_t rows, cols, nnz;
    size_t *offset, *column;
    double *values;
} mulib_num_csr_t;
int mulib_num_csr_from_triplets(const mulib_num_sparse_t *s, mulib_num_csr_t *out);
int mulib_num_csr_matvec(const mulib_num_csr_t *s, const double *x, double *y);
void mulib_num_csr_free(mulib_num_csr_t *s);
int mulib_num_csr_validate(const mulib_num_csr_t *s);

int mulib_num_csr_transpose(const mulib_num_csr_t *s, mulib_num_csr_t *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_NUM_CSR_H */
#if defined(MULIB_IMPL_NUM_CSR) && !defined(MULIB_NUM_CSR_H_IMPLEMENTED)
#define MULIB_NUM_CSR_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
void mulib_num_csr_free(mulib_num_csr_t *s) {
    if (!s)
        return;
    free(s->offset);
    free(s->column);
    free(s->values);
    memset(s, 0, sizeof *s);
}
int mulib_num_csr_from_triplets(const mulib_num_sparse_t *s, mulib_num_csr_t *out) {
    if (!mulib_num_sparse_validate(s) || !out || s->count > SIZE_MAX / sizeof(double) ||
        s->rows == SIZE_MAX || s->count > SIZE_MAX / sizeof(size_t) ||
        s->rows > SIZE_MAX / sizeof(size_t) - 1)
        return 0;
    mulib_num_csr_t z = {0};
    z.rows = s->rows;
    z.cols = s->cols;
    z.nnz = s->count;
    z.offset = calloc(s->rows + 1, sizeof(size_t));
    if (!z.offset)
        return 0;
    if (s->count) {
        z.column = malloc(s->count * sizeof(size_t));
        z.values = malloc(s->count * sizeof(double));
        if (!z.column || !z.values) {
            mulib_num_csr_free(&z);
            return 0;
        }
    }
    for (size_t i = 0; i < s->count; i++)
        z.offset[s->triplets[i].row + 1]++;
    for (size_t i = 1; i <= z.rows; i++)
        z.offset[i] += z.offset[i - 1];
    size_t *cur = malloc(z.rows * sizeof(size_t));
    if (z.rows && !cur) {
        mulib_num_csr_free(&z);
        return 0;
    }
    if (z.rows)
        memcpy(cur, z.offset, z.rows * sizeof(size_t));
    for (size_t i = 0; i < s->count; i++) {
        size_t k = cur[s->triplets[i].row]++;
        z.column[k] = s->triplets[i].col;
        z.values[k] = s->triplets[i].value;
    }
    free(cur);
    for (size_t j = 0; j < z.rows; j++)
        for (size_t k = z.offset[j] + (z.offset[j] < z.offset[j + 1]); k < z.offset[j + 1]; k++) {
            size_t idx = z.column[k], q = k;
            double v = z.values[k];
            while (q > z.offset[j] && z.column[q - 1] > idx) {
                z.column[q] = z.column[q - 1];
                z.values[q] = z.values[q - 1];
                q--;
            }
            z.column[q] = idx;
            z.values[q] = v;
        }
    *out = z;
    return 1;
}
int mulib_num_csr_matvec(const mulib_num_csr_t *s, const double *x, double *y) {
    if (!mulib_num_csr_validate(s) || !x || !y || s->rows > SIZE_MAX / sizeof(double))
        return 0;
    double *r = calloc(s->rows ? s->rows : 1, sizeof(double));
    if (!r)
        return 0;
    int ok = 1;
    for (size_t j = 0; j < s->rows && ok; j++)
        for (size_t k = s->offset[j]; k < s->offset[j + 1]; k++) {
            size_t row = j;
            r[row] += s->values[k] * x[s->column[k]];
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
int mulib_num_csr_validate(const mulib_num_csr_t *s) {
    if (!s || !s->offset || s->rows == SIZE_MAX || (s->nnz && (!s->column || !s->values)) ||
        s->offset[0] != 0 || s->offset[s->rows] != s->nnz)
        return 0;
    for (size_t j = 0; j < s->rows; j++)
        if (s->offset[j] > s->offset[j + 1] || s->offset[j + 1] > s->nnz)
            return 0;
    for (size_t k = 0; k < s->nnz; k++)
        if (s->column[k] >= s->cols || !isfinite(s->values[k]))
            return 0;
    return 1;
}

int mulib_num_csr_transpose(const mulib_num_csr_t *s, mulib_num_csr_t *out) {
    if (!out || s == out || !mulib_num_csr_validate(s) || s->cols > SIZE_MAX / sizeof(size_t) - 1 ||
        s->nnz > SIZE_MAX / sizeof(double) || s->nnz > SIZE_MAX / sizeof(size_t))
        return 0;
    mulib_num_csr_t r = {0};
    r.rows = s->cols;
    r.cols = s->rows;
    r.nnz = s->nnz;
    r.offset = calloc(r.rows + 1, sizeof *r.offset);
    if (!r.offset)
        return 0;
    if (r.nnz) {
        r.column = malloc(r.nnz * sizeof *r.column);
        r.values = malloc(r.nnz * sizeof *r.values);
        if (!r.column || !r.values) {
            mulib_num_csr_free(&r);
            return 0;
        }
    }
    size_t *cursor = r.rows ? calloc(r.rows, sizeof *cursor) : NULL;
    if (r.rows && !cursor) {
        mulib_num_csr_free(&r);
        return 0;
    }
    for (size_t k = 0; k < s->nnz; k++)
        r.offset[s->column[k] + 1]++;
    for (size_t j = 0; j < r.rows; j++) {
        r.offset[j + 1] += r.offset[j];
        cursor[j] = r.offset[j];
    }
    for (size_t j = 0; j < s->rows; j++)
        for (size_t k = s->offset[j]; k < s->offset[j + 1]; k++) {
            size_t at = cursor[s->column[k]]++;
            r.column[at] = j;
            r.values[at] = s->values[k];
        }
    free(cursor);
    *out = r;
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_NUM_CSR */
