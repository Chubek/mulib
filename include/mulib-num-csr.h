/** @file mulib-num-csr.h
 * @brief Compressed sparse row conversion, validation, matvec and transpose.
 * @details C11 single-header implementation; define MULIB_IMPL_NUM_CSR in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_NUM_CSR_H
#define MULIB_NUM_CSR_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-amb3r-base.h"
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

/* Counts are readable ELEMENTS in each source array, not bytes:
 * at least rows+1 offsets, nnz columns and nnz values. Offset storage is
 * required even for zero rows; column/value pointers may be NULL only with
 * zero corresponding counts (and thus nnz=0).
 * Indices are zero-based; values are finite double coefficients with no unit
 * conversion. Both adapters preserve row order, duplicates and explicit zeros.
 * Source arrays must be live and unmodified during validation/copy; out must
 * be writable and disjoint from the source object and arrays. Return 1 on
 * success, 0 on invalid input or allocation failure, leaving *out unchanged. */
/* Copies the source into a zero-initialized destination. Free only the copy
 * with mulib_num_csr_free; the source keeps its original ownership. */
int mulib_num_csr_from_amb3r(const ma_csr *s, size_t offset_count, size_t column_count,
                            size_t value_count, mulib_num_csr_t *out);
/* Allocation-free read-only borrow. Keep the source arrays alive and immutable
 * until the last use of the borrow; never free through the view. */
int mulib_num_csr_borrow_amb3r(const mulib_num_csr_t *s, size_t offset_count,
                              size_t column_count, size_t value_count, ma_csr *out);

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
static int mulib_num_csr_alloc_(size_t rows, size_t cols, size_t nnz, mulib_num_csr_t *out) {
    if (rows > SIZE_MAX / sizeof(size_t) - 1 ||
        nnz > SIZE_MAX / sizeof(size_t) || nnz > SIZE_MAX / sizeof(double))
        return 0;
    mulib_num_csr_t z = {0};
    z.rows = rows;
    z.cols = cols;
    z.nnz = nnz;
    z.offset = calloc(rows + 1, sizeof *z.offset);
    if (!z.offset)
        return 0;
    if (nnz) {
        z.column = malloc(nnz * sizeof *z.column);
        z.values = malloc(nnz * sizeof *z.values);
        if (!z.column || !z.values) {
            mulib_num_csr_free(&z);
            return 0;
        }
    }
    *out = z;
    return 1;
}
static ma_csr mulib_num_csr_view_(const mulib_num_csr_t *s) {
    ma_csr v = {s->rows, s->cols, s->nnz, s->offset, s->column, s->values};
    return v;
}
static int mulib_num_csr_view_valid_(const ma_csr *s, size_t offset_count,
                                     size_t column_count, size_t value_count) {
    /* rows < offset_count checks rows+1 without adding to an untrusted count. */
    return s && s->rows < offset_count && offset_count <= SIZE_MAX / sizeof *s->ptr &&
           s->nnz <= column_count && column_count <= SIZE_MAX / sizeof *s->idx &&
           (s->idx || !column_count) &&
           s->nnz <= value_count && value_count <= SIZE_MAX / sizeof *s->val &&
           (s->val || !value_count) && ma_csr_valid(s);
}
int mulib_num_csr_from_amb3r(const ma_csr *s, size_t offset_count, size_t column_count,
                            size_t value_count, mulib_num_csr_t *out) {
    if (!out || out->rows || out->cols || out->nnz || out->offset || out->column || out->values ||
        !mulib_num_csr_view_valid_(s, offset_count, column_count, value_count))
        return 0;
    mulib_num_csr_t z;
    if (!mulib_num_csr_alloc_(s->rows, s->cols, s->nnz, &z))
        return 0;
    memcpy(z.offset, s->ptr, (s->rows + 1) * sizeof *z.offset);
    if (s->nnz) {
        memcpy(z.column, s->idx, s->nnz * sizeof *z.column);
        memcpy(z.values, s->val, s->nnz * sizeof *z.values);
    }
    *out = z;
    return 1;
}
int mulib_num_csr_borrow_amb3r(const mulib_num_csr_t *s, size_t offset_count,
                              size_t column_count, size_t value_count, ma_csr *out) {
    if (!s || !out)
        return 0;
    ma_csr v = mulib_num_csr_view_(s);
    if (!mulib_num_csr_view_valid_(&v, offset_count, column_count, value_count))
        return 0;
    *out = v;
    return 1;
}
int mulib_num_csr_from_triplets(const mulib_num_sparse_t *s, mulib_num_csr_t *out) {
    mulib_num_csr_t z;
    if (!out || !mulib_num_sparse_validate(s) ||
        !mulib_num_csr_alloc_(s->rows, s->cols, s->count, &z))
        return 0;
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
    if (!s)
        return 0;
    ma_csr v = mulib_num_csr_view_(s);
    return ma_csr_valid(&v);
}

int mulib_num_csr_transpose(const mulib_num_csr_t *s, mulib_num_csr_t *out) {
    mulib_num_csr_t r;
    if (!out || s == out || !mulib_num_csr_validate(s) ||
        !mulib_num_csr_alloc_(s->cols, s->rows, s->nnz, &r))
        return 0;
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
