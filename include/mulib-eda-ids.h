/** @file mulib-eda-ids.h
 * @brief Typed 64-bit IDs: 8-bit kind, 24-bit generation and 32-bit index plus one; stale
 * generations are rejected.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_IDS in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_IDS_H
#define MULIB_EDA_IDS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t mulib_eda_id_t;
typedef struct mulib_eda_id_pool {
    uint32_t *generation;
    uint8_t *active;
    uint8_t *kind;
    size_t length, capacity;
} mulib_eda_id_pool_t;
int mulib_eda_id_pool_init(mulib_eda_id_pool_t *p, size_t initial_capacity);
void mulib_eda_id_pool_free(mulib_eda_id_pool_t *p);
int mulib_eda_id_create(mulib_eda_id_pool_t *p, uint8_t kind, mulib_eda_id_t *out);
int mulib_eda_id_valid(const mulib_eda_id_pool_t *p, mulib_eda_id_t id, uint8_t kind);
int mulib_eda_id_remove(mulib_eda_id_pool_t *p, mulib_eda_id_t id);
uint32_t mulib_eda_id_index(mulib_eda_id_t id);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_IDS_H */
#if defined(MULIB_IMPL_EDA_IDS) && !defined(MULIB_EDA_IDS_H_IMPLEMENTED)
#define MULIB_EDA_IDS_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t mulib_eda_id_index(mulib_eda_id_t id) {
    uint32_t n = (uint32_t)id;
    return n ? n - 1 : UINT32_MAX;
}
int mulib_eda_id_pool_init(mulib_eda_id_pool_t *p, size_t c) {
    if (!p || c > UINT32_MAX || c > SIZE_MAX / sizeof(uint32_t))
        return 0;
    memset(p, 0, sizeof *p);
    if (!c)
        return 1;
    p->generation = calloc(c, sizeof(uint32_t));
    p->active = calloc(c, 1);
    p->kind = calloc(c, 1);
    if (!p->generation || !p->active || !p->kind) {
        mulib_eda_id_pool_free(p);
        return 0;
    }
    p->capacity = c;
    return 1;
}
void mulib_eda_id_pool_free(mulib_eda_id_pool_t *p) {
    if (!p)
        return;
    free(p->generation);
    free(p->active);
    free(p->kind);
    memset(p, 0, sizeof *p);
}
int mulib_eda_id_create(mulib_eda_id_pool_t *p, uint8_t kind, mulib_eda_id_t *out) {
    if (!p || !out || !kind)
        return 0;
    size_t i = 0;
    for (; i < p->length; i++)
        if (!p->active[i] && p->generation[i] < 0xffffffu)
            break;
    if (i == p->length) {
        if (i == UINT32_MAX)
            return 0;
        if (i == p->capacity) {
            size_t n = p->capacity ? p->capacity * 2 : 8;
            if (n <= p->capacity || n > UINT32_MAX)
                n = UINT32_MAX;
            if (n <= i || n > SIZE_MAX / sizeof(uint32_t))
                return 0;
            uint32_t *g = calloc(n, sizeof *g);
            uint8_t *a = calloc(n, 1), *k = calloc(n, 1);
            if (!g || !a || !k) {
                free(g);
                free(a);
                free(k);
                return 0;
            }
            if (i) {
                memcpy(g, p->generation, i * sizeof *g);
                memcpy(a, p->active, i);
                memcpy(k, p->kind, i);
            }
            free(p->generation);
            free(p->active);
            free(p->kind);
            p->generation = g;
            p->active = a;
            p->kind = k;
            p->capacity = n;
        }
        p->length++;
    }
    if (!p->generation[i]) {
        p->generation[i] = 1;
    }
    p->active[i] = 1;
    p->kind[i] = kind;
    *out = ((uint64_t)kind << 56) | ((uint64_t)p->generation[i] << 32) | (uint64_t)(i + 1);
    return 1;
}
int mulib_eda_id_valid(const mulib_eda_id_pool_t *p, mulib_eda_id_t id, uint8_t kind) {
    if (!p || !id)
        return 0;
    uint32_t idx = mulib_eda_id_index(id);
    uint8_t k = (uint8_t)(id >> 56);
    uint32_t gen = (uint32_t)((id >> 32) & 0xffffffu);
    return idx < p->length && k != 0 && (!kind || k == kind) && p->active[idx] &&
           p->kind[idx] == k && p->generation[idx] == gen;
}
int mulib_eda_id_remove(mulib_eda_id_pool_t *p, mulib_eda_id_t id) {
    if (!mulib_eda_id_valid(p, id, 0))
        return 0;
    size_t i = mulib_eda_id_index(id);
    p->active[i] = 0;
    p->generation[i]++;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_EDA_IDS */
