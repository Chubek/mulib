/** @file mulib-eda-hierarchy.h
 * @brief Append-only hierarchy with unique sibling names and slash-separated paths.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_HIERARCHY in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_HIERARCHY_H
#define MULIB_EDA_HIERARCHY_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_hnode {
    uint32_t parent;
    char name[48];
} mulib_eda_hnode_t;
typedef struct mulib_eda_hierarchy {
    mulib_eda_hnode_t *nodes;
    size_t count, capacity;
} mulib_eda_hierarchy_t;
int mulib_eda_hierarchy_add(mulib_eda_hierarchy_t *h, uint32_t parent, const char *name,
                            uint32_t *out);
int mulib_eda_hierarchy_path(const mulib_eda_hierarchy_t *h, uint32_t id, char *dst, size_t len);
void mulib_eda_hierarchy_free(mulib_eda_hierarchy_t *h);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_HIERARCHY_H */
#if defined(MULIB_IMPL_EDA_HIERARCHY) && !defined(MULIB_EDA_HIERARCHY_H_IMPLEMENTED)
#define MULIB_EDA_HIERARCHY_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
void mulib_eda_hierarchy_free(mulib_eda_hierarchy_t *h) {
    if (!h)
        return;
    free(h->nodes);
    memset(h, 0, sizeof *h);
}
int mulib_eda_hierarchy_add(mulib_eda_hierarchy_t *h, uint32_t parent, const char *name,
                            uint32_t *out) {
    if (!h || !name || !name[0] || strchr(name, '/') || strlen(name) >= 48 ||
        h->count >= UINT32_MAX || ((parent != UINT32_MAX) && parent >= h->count))
        return 0;
    for (size_t i = 0; i < h->count; i++)
        if (h->nodes[i].parent == parent && !strcmp(name, h->nodes[i].name))
            return 0;
    if (h->count == h->capacity) {
        size_t n = h->capacity ? h->capacity * 2 : 8;
        if (n <= h->capacity || n > SIZE_MAX / sizeof *h->nodes)
            return 0;
        void *p = realloc(h->nodes, n * sizeof *h->nodes);
        if (!p)
            return 0;
        h->nodes = p;
        h->capacity = n;
    }
    uint32_t id = (uint32_t)h->count++;
    h->nodes[id].parent = parent;
    strcpy(h->nodes[id].name, name);
    if (out)
        *out = id;
    return 1;
}
int mulib_eda_hierarchy_path(const mulib_eda_hierarchy_t *h, uint32_t id, char *dst, size_t n) {
    if (!h || !h->nodes || id >= h->count || !dst || !n)
        return 0;
    size_t length = 0, depth = 0;
    uint32_t at = id;
    while (at != UINT32_MAX) {
        if (at >= h->count || depth++ >= h->count)
            return 0;
        const char *name = h->nodes[at].name;
        const char *end = memchr(name, 0, sizeof h->nodes[at].name);
        if (!end)
            return 0;
        size_t k = (size_t)(end - name);
        if (k >= SIZE_MAX - length)
            return 0;
        length += k + 1;
        at = h->nodes[at].parent;
    }
    if (length > n)
        return 0;
    dst[length - 1] = 0;
    size_t pos = length - 1;
    at = id;
    while (at != UINT32_MAX) {
        size_t k = strlen(h->nodes[at].name);
        pos -= k;
        memcpy(dst + pos, h->nodes[at].name, k);
        at = h->nodes[at].parent;
        if (at != UINT32_MAX)
            dst[--pos] = '/';
    }
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_EDA_HIERARCHY */
