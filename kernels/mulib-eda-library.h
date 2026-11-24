/** @file mulib-eda-library.h
 * @brief In-memory part definition catalog with immutable key, description and pin count.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_LIBRARY in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_LIBRARY_H
#define MULIB_EDA_LIBRARY_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_partdef {
    char key[48], description[96];
    uint16_t pins;
} mulib_eda_partdef_t;
typedef struct mulib_eda_library {
    mulib_eda_partdef_t *parts;
    size_t count, cap;
} mulib_eda_library_t;
int mulib_eda_library_register(mulib_eda_library_t *lib, const mulib_eda_partdef_t *part);
const mulib_eda_partdef_t *mulib_eda_library_find(const mulib_eda_library_t *lib, const char *key);
void mulib_eda_library_free(mulib_eda_library_t *lib);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_LIBRARY_H */
#if defined(MULIB_IMPL_EDA_LIBRARY) && !defined(MULIB_EDA_LIBRARY_H_IMPLEMENTED)
#define MULIB_EDA_LIBRARY_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
void mulib_eda_library_free(mulib_eda_library_t *l) {
    if (!l)
        return;
    free(l->parts);
    memset(l, 0, sizeof *l);
}
const mulib_eda_partdef_t *mulib_eda_library_find(const mulib_eda_library_t *l, const char *key) {
    if (!l || !key)
        return NULL;
    for (size_t i = 0; i < l->count; i++)
        if (!strcmp(l->parts[i].key, key))
            return l->parts + i;
    return NULL;
}
int mulib_eda_library_register(mulib_eda_library_t *l, const mulib_eda_partdef_t *p) {
    if (!l || !p || !memchr(p->key, 0, sizeof p->key) || !p->key[0] ||
        !memchr(p->description, 0, sizeof p->description) || !p->pins ||
        mulib_eda_library_find(l, p->key))
        return 0;
    if (l->count == l->cap) {
        size_t n = l->cap ? l->cap * 2 : 16;
        if (n <= l->cap || n > SIZE_MAX / sizeof *l->parts)
            return 0;
        void *q = realloc(l->parts, n * sizeof *l->parts);
        if (!q)
            return 0;
        l->parts = q;
        l->cap = n;
    }
    l->parts[l->count++] = *p;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_EDA_LIBRARY */
