/** @file mulib-eda-undo.h
 * @brief Bounded 256-command reversible scalar-field journal over eda-design-db.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_UNDO in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_UNDO_H
#define MULIB_EDA_UNDO_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-eda-design-db.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_undo_entry {
    uint64_t id;
    double before, after;
} mulib_eda_undo_entry_t;
typedef struct mulib_eda_undo {
    mulib_eda_undo_entry_t entries[256];
    size_t count, cursor;
} mulib_eda_undo_t;
int mulib_eda_undo_apply(mulib_eda_undo_t *u, mulib_eda_design_db_t *db, uint64_t id, double after);
int mulib_eda_undo_back(mulib_eda_undo_t *u, mulib_eda_design_db_t *db);
int mulib_eda_undo_forward(mulib_eda_undo_t *u, mulib_eda_design_db_t *db);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_UNDO_H */
#if defined(MULIB_IMPL_EDA_UNDO) && !defined(MULIB_EDA_UNDO_H_IMPLEMENTED)
#define MULIB_EDA_UNDO_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_undo_apply(mulib_eda_undo_t *u, mulib_eda_design_db_t *db, uint64_t id,
                         double after) {
    if (!u || !db || !isfinite(after) || u->cursor >= 256)
        return 0;
    mulib_eda_db_record_t r;
    if (!mulib_eda_db_get(db, id, &r))
        return 0;
    double before = r.value;
    r.value = after;
    if (!mulib_eda_db_put(db, r))
        return 0;
    u->count = u->cursor;
    u->entries[u->cursor++] = (mulib_eda_undo_entry_t){id, before, after};
    u->count = u->cursor;
    return 1;
}
int mulib_eda_undo_back(mulib_eda_undo_t *u, mulib_eda_design_db_t *db) {
    if (!u || !db || !u->cursor)
        return 0;
    mulib_eda_undo_entry_t e = u->entries[u->cursor - 1];
    mulib_eda_db_record_t r;
    if (!mulib_eda_db_get(db, e.id, &r) || r.value != e.after)
        return 0;
    r.value = e.before;
    if (!mulib_eda_db_put(db, r))
        return 0;
    u->cursor--;
    return 1;
}
int mulib_eda_undo_forward(mulib_eda_undo_t *u, mulib_eda_design_db_t *db) {
    if (!u || !db || u->cursor == u->count)
        return 0;
    mulib_eda_undo_entry_t e = u->entries[u->cursor];
    mulib_eda_db_record_t r;
    if (!mulib_eda_db_get(db, e.id, &r) || r.value != e.before)
        return 0;
    r.value = e.after;
    if (!mulib_eda_db_put(db, r))
        return 0;
    u->cursor++;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_EDA_UNDO */
