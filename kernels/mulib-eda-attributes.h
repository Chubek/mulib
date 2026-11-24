/** @file mulib-eda-attributes.h
 * @brief Owned typed attribute table with integer, finite real and bounded byte-string values,
 * replacement, lookup and removal.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_ATTRIBUTES in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_ATTRIBUTES_H
#define MULIB_EDA_ATTRIBUTES_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_eda_attr_type {
    MULIB_EDA_ATTR_INTEGER = 1,
    MULIB_EDA_ATTR_REAL = 2,
    MULIB_EDA_ATTR_STRING = 3
} mulib_eda_attr_type_t;
typedef struct mulib_eda_attr {
    uint64_t owner;
    char key[48];
    mulib_eda_attr_type_t type;
    int64_t integer;
    double real;
    char text[128];
} mulib_eda_attr_t;
typedef struct mulib_eda_attrs {
    mulib_eda_attr_t *data;
    size_t count, cap;
} mulib_eda_attrs_t;
int mulib_eda_attrs_set(mulib_eda_attrs_t *a, const mulib_eda_attr_t *value);
const mulib_eda_attr_t *mulib_eda_attrs_get(const mulib_eda_attrs_t *a, uint64_t owner,
                                            const char *key);
void mulib_eda_attrs_free(mulib_eda_attrs_t *a);
int mulib_eda_attrs_remove(mulib_eda_attrs_t *a, uint64_t owner, const char *key);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_ATTRIBUTES_H */
#if defined(MULIB_IMPL_EDA_ATTRIBUTES) && !defined(MULIB_EDA_ATTRIBUTES_H_IMPLEMENTED)
#define MULIB_EDA_ATTRIBUTES_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
void mulib_eda_attrs_free(mulib_eda_attrs_t *a) {
    if (!a)
        return;
    free(a->data);
    memset(a, 0, sizeof *a);
}
const mulib_eda_attr_t *mulib_eda_attrs_get(const mulib_eda_attrs_t *a, uint64_t id,
                                            const char *key) {
    if (!a || !key)
        return NULL;
    for (size_t i = 0; i < a->count; i++)
        if (a->data[i].owner == id && !strcmp(a->data[i].key, key))
            return &a->data[i];
    return NULL;
}
int mulib_eda_attrs_set(mulib_eda_attrs_t *a, const mulib_eda_attr_t *v) {
    if (!a || !v || !v->owner || !v->key[0] || !memchr(v->key, 0, sizeof v->key) ||
        !memchr(v->text, 0, sizeof v->text) || v->type < 1 || v->type > 3 ||
        (v->type == 2 && !isfinite(v->real)))
        return 0;
    for (size_t i = 0; i < a->count; i++)
        if (a->data[i].owner == v->owner && !strcmp(a->data[i].key, v->key)) {
            a->data[i] = *v;
            return 1;
        }
    if (a->count == a->cap) {
        size_t n = a->cap ? a->cap * 2 : 8;
        if (n <= a->cap || n > SIZE_MAX / sizeof *a->data)
            return 0;
        void *p = realloc(a->data, n * sizeof *a->data);
        if (!p)
            return 0;
        a->data = p;
        a->cap = n;
    }
    a->data[a->count++] = *v;
    return 1;
}
int mulib_eda_attrs_remove(mulib_eda_attrs_t *a, uint64_t owner, const char *key) {
    if (!a || !key)
        return 0;
    for (size_t i = 0; i < a->count; i++)
        if (a->data[i].owner == owner && !strcmp(a->data[i].key, key)) {
            memmove(a->data + i, a->data + i + 1, (a->count - i - 1) * sizeof *a->data);
            a->count--;
            return 1;
        }
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_EDA_ATTRIBUTES */
