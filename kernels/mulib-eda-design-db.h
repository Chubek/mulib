/** @file mulib-eda-design-db.h
 * @brief Flat revisioned design store keyed by nonzero external IDs, with typed finite scalar
 * values and independent owned snapshot cloning.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_DESIGN_DB in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_DESIGN_DB_H
#define MULIB_EDA_DESIGN_DB_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_db_record {
    uint64_t id;
    uint32_t type;
    double value;
} mulib_eda_db_record_t;
typedef struct mulib_eda_design_db {
    mulib_eda_db_record_t *items;
    size_t count, capacity;
    uint64_t epoch;
} mulib_eda_design_db_t;
int mulib_eda_db_put(mulib_eda_design_db_t *db, mulib_eda_db_record_t record);
int mulib_eda_db_get(const mulib_eda_design_db_t *db, uint64_t id, mulib_eda_db_record_t *out);
int mulib_eda_db_delete(mulib_eda_design_db_t *db, uint64_t id);
void mulib_eda_db_free(mulib_eda_design_db_t *db);
/* Clone into an empty destination. On failure destination is unchanged. */
int mulib_eda_db_clone(const mulib_eda_design_db_t *source, mulib_eda_design_db_t *destination);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_DESIGN_DB_H */
#if defined(MULIB_IMPL_EDA_DESIGN_DB) && !defined(MULIB_EDA_DESIGN_DB_H_IMPLEMENTED)
#define MULIB_EDA_DESIGN_DB_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
void mulib_eda_db_free(mulib_eda_design_db_t *db) {
    if (!db)
        return;
    free(db->items);
    memset(db, 0, sizeof *db);
}
int mulib_eda_db_get(const mulib_eda_design_db_t *db, uint64_t id, mulib_eda_db_record_t *out) {
    if (!db || !id || !out)
        return 0;
    for (size_t i = 0; i < db->count; i++)
        if (db->items[i].id == id) {
            *out = db->items[i];
            return 1;
        }
    return 0;
}
int mulib_eda_db_put(mulib_eda_design_db_t *db, mulib_eda_db_record_t rec) {
    if (!db || !rec.id || !rec.type || !isfinite(rec.value) || db->epoch == UINT64_MAX)
        return 0;
    for (size_t i = 0; i < db->count; i++)
        if (db->items[i].id == rec.id) {
            db->items[i] = rec;
            db->epoch++;
            return 1;
        }
    if (db->count == db->capacity) {
        size_t n = db->capacity ? db->capacity * 2 : 16;
        if (n <= db->capacity || n > SIZE_MAX / sizeof *db->items)
            return 0;
        void *p = realloc(db->items, n * sizeof *db->items);
        if (!p)
            return 0;
        db->items = p;
        db->capacity = n;
    }
    db->items[db->count++] = rec;
    db->epoch++;
    return 1;
}
int mulib_eda_db_delete(mulib_eda_design_db_t *db, uint64_t id) {
    if (!db || !id || db->epoch == UINT64_MAX)
        return 0;
    for (size_t i = 0; i < db->count; i++)
        if (db->items[i].id == id) {
            memmove(db->items + i, db->items + i + 1, (db->count - i - 1) * sizeof *db->items);
            db->count--;
            db->epoch++;
            return 1;
        }
    return 0;
}
int mulib_eda_db_clone(const mulib_eda_design_db_t *src, mulib_eda_design_db_t *dst) {
    if (!src || !dst || src == dst || dst->items || dst->count || dst->capacity ||
        src->count > src->capacity || src->count > SIZE_MAX / sizeof *src->items ||
        (src->count && !src->items))
        return 0;
    mulib_eda_design_db_t r = {0};
    if (src->count) {
        r.items = malloc(src->count * sizeof *r.items);
        if (!r.items)
            return 0;
        memcpy(r.items, src->items, src->count * sizeof *r.items);
    }
    r.count = r.capacity = src->count;
    r.epoch = src->epoch;
    *dst = r;
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_EDA_DESIGN_DB */
