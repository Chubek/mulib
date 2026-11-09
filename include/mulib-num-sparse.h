/** @file mulib-num-sparse.h
 * @brief Owned coordinate-list sparse double matrix with checked assembly, repeated-coordinate
 * accumulation and explicit canonicalization.
 * @details C11 single-header implementation; define MULIB_IMPL_NUM_SPARSE in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_NUM_SPARSE_H
#define MULIB_NUM_SPARSE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_num_triplet {
    size_t row, col;
    double value;
} mulib_num_triplet_t;
typedef struct mulib_num_sparse {
    size_t rows, cols, count, capacity;
    mulib_num_triplet_t *triplets;
} mulib_num_sparse_t;
int mulib_num_sparse_init(mulib_num_sparse_t *s, size_t rows, size_t cols);
int mulib_num_sparse_add(mulib_num_sparse_t *s, size_t row, size_t col, double value);
int mulib_num_sparse_matvec(const mulib_num_sparse_t *s, const double *x, double *y);
void mulib_num_sparse_free(mulib_num_sparse_t *s);
int mulib_num_sparse_validate(const mulib_num_sparse_t *s);
int mulib_num_sparse_canonicalize(mulib_num_sparse_t *s);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_NUM_SPARSE_H */
#if defined(MULIB_IMPL_NUM_SPARSE) && !defined(MULIB_NUM_SPARSE_H_IMPLEMENTED)
#define MULIB_NUM_SPARSE_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_sparse_init(mulib_num_sparse_t *s, size_t r, size_t c) {
    if (!s)
        return 0;
    memset(s, 0, sizeof *s);
    s->rows = r;
    s->cols = c;
    return 1;
}
void mulib_num_sparse_free(mulib_num_sparse_t *s) {
    if (!s)
        return;
    free(s->triplets);
    memset(s, 0, sizeof *s);
}
int mulib_num_sparse_add(mulib_num_sparse_t *s, size_t r, size_t c, double x) {
    if (!s || s->count > s->capacity || (s->count && !s->triplets) || r >= s->rows ||
        c >= s->cols || !isfinite(x))
        return 0;
    if (!x)
        return 1;
    for (size_t i = 0; i < s->count; i++)
        if (s->triplets[i].row == r && s->triplets[i].col == c) {
            double v = s->triplets[i].value + x;
            if (!isfinite(v))
                return 0;
            s->triplets[i].value = v;
            return 1;
        }
    if (s->count == s->capacity) {
        size_t n = s->capacity ? s->capacity * 2 : 8;
        if (n <= s->capacity || n > SIZE_MAX / sizeof *s->triplets)
            return 0;
        void *p = realloc(s->triplets, n * sizeof *s->triplets);
        if (!p)
            return 0;
        s->triplets = p;
        s->capacity = n;
    }
    s->triplets[s->count++] = (mulib_num_triplet_t){r, c, x};
    return 1;
}
int mulib_num_sparse_matvec(const mulib_num_sparse_t *s, const double *x, double *y) {
    if (!mulib_num_sparse_validate(s) || !x || !y || s->rows > SIZE_MAX / sizeof(double))
        return 0;
    double *result = calloc(s->rows ? s->rows : 1, sizeof(double));
    if (!result)
        return 0;
    int ok = 1;
    for (size_t i = 0; i < s->count; i++) {
        mulib_num_triplet_t t = s->triplets[i];
        result[t.row] += t.value * x[t.col];
        if (!isfinite(result[t.row])) {
            ok = 0;
            break;
        }
    }
    if (ok && s->rows)
        memcpy(y, result, s->rows * sizeof(double));
    free(result);
    return ok;
}
int mulib_num_sparse_validate(const mulib_num_sparse_t *s) {
    if (!s || s->count > s->capacity || (s->count && !s->triplets))
        return 0;
    for (size_t i = 0; i < s->count; i++)
        if (s->triplets[i].row >= s->rows || s->triplets[i].col >= s->cols ||
            !isfinite(s->triplets[i].value))
            return 0;
    return 1;
}
static int mulib_num_triplet_compare_(const void *a, const void *b) {
    const mulib_num_triplet_t *x = a, *y = b;
    if (x->row != y->row)
        return x->row < y->row ? -1 : 1;
    return x->col < y->col ? -1 : x->col > y->col;
}
int mulib_num_sparse_canonicalize(mulib_num_sparse_t *s) {
    if (!mulib_num_sparse_validate(s) || s->count > SIZE_MAX / sizeof *s->triplets)
        return 0;
    if (!s->count)
        return 1;
    mulib_num_triplet_t *copy = malloc(s->count * sizeof *copy);
    if (!copy)
        return 0;
    memcpy(copy, s->triplets, s->count * sizeof *copy);
    qsort(copy, s->count, sizeof *copy, mulib_num_triplet_compare_);
    size_t n = 0;
    for (size_t i = 0; i < s->count; i++) {
        if (n && copy[n - 1].row == copy[i].row && copy[n - 1].col == copy[i].col) {
            double v = copy[n - 1].value + copy[i].value;
            if (!isfinite(v)) {
                free(copy);
                return 0;
            }
            copy[n - 1].value = v;
        } else
            copy[n++] = copy[i];
    }
    size_t k = 0;
    for (size_t i = 0; i < n; i++)
        if (copy[i].value != 0)
            copy[k++] = copy[i];
    memcpy(s->triplets, copy, k * sizeof *copy);
    s->count = k;
    free(copy);
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_NUM_SPARSE */
