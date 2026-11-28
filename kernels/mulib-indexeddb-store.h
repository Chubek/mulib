/* MuLIB browser staging addition: indexeddb-store
 * Implemented subset: Bounded ordered borrowed records and atomic in-memory batches.
 * Remaining: Persistence, structured clone, key generators, transaction isolation.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_INDEXEDDB_STORE_H
#define MULIB_INDEXEDDB_STORE_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-indexeddb-key.h"
typedef struct {mulib_indexeddb_key_t key;uintptr_t value;} mulib_indexeddb_store_record;
typedef struct {mulib_indexeddb_store_record *v;size_t len,cap;} mulib_indexeddb_store_t;
/* Start with len == 0. Live keys must remain valid and immutable. */
static inline int mulib_indexeddb_store_shape_(const mulib_indexeddb_store_t *s)
{
    return s && s->len <= s->cap && (!s->cap || s->v) &&
           s->cap <= SIZE_MAX / sizeof *s->v;
}

/* Find the first equal or greater key in a store maintained by this API. */
static inline int mulib_indexeddb_store_locate_(const mulib_indexeddb_store_t *s,
    mulib_indexeddb_key_t key, size_t *pos, int *found)
{
    if (!mulib_indexeddb_store_shape_(s) || !mulib_indexeddb_key_valid(key))
        return 0;
    size_t lo = 0, hi = s->len;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int cmp;
        if (!mulib_indexeddb_key_compare(s->v[mid].key, key, &cmp))
            return 0;
        if (!cmp) {
            *pos = mid;
            *found = 1;
            return 1;
        }
        if (cmp < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    *pos = lo;
    *found = 0;
    return 1;
}

static inline int mulib_indexeddb_store_put(mulib_indexeddb_store_t *s,
    mulib_indexeddb_key_t key, uintptr_t val, int overwrite)
{
    size_t i;
    int found;
    if (!mulib_indexeddb_store_locate_(s, key, &i, &found))
        return 0;
    if (found) {
        if (!overwrite)
            return 0;
        s->v[i].value = val;
        return 1;
    }
    if (s->len == s->cap)
        return 0;
    if (i < s->len)
        memmove(s->v + i + 1, s->v + i, (s->len - i) * sizeof *s->v);
    s->v[i].key = key;
    s->v[i].value = val;
    s->len++;
    return 1;
}

static inline int mulib_indexeddb_store_get(const mulib_indexeddb_store_t *s,
    mulib_indexeddb_key_t key, uintptr_t *out)
{
    size_t i;
    int found;
    if (!out || !mulib_indexeddb_store_locate_(s, key, &i, &found) || !found)
        return 0;
    *out = s->v[i].value;
    return 1;
}

/* Removing a missing but valid key succeeds without changing the store. */
static inline int mulib_indexeddb_store_delete(mulib_indexeddb_store_t *s,
    mulib_indexeddb_key_t key)
{
    size_t i;
    int found;
    if (!mulib_indexeddb_store_locate_(s, key, &i, &found))
        return 0;
    if (found) {
        if (i + 1 < s->len)
            memmove(s->v + i, s->v + i + 1, (s->len - i - 1) * sizeof *s->v);
        s->len--;
    }
    return 1;
}

typedef enum {
    mulib_indexeddb_store_ADD,
    mulib_indexeddb_store_PUT,
    mulib_indexeddb_store_DELETE
} mulib_indexeddb_store_op_kind;

typedef struct {
    mulib_indexeddb_store_op_kind kind;
    mulib_indexeddb_key_t key;
    uintptr_t value; /* Ignored for DELETE. */
} mulib_indexeddb_store_op;

/* scratch must provide s->cap records, disjoint from s, s->v, ops, and
 * all key bytes. ops and key bytes must not alias either record array.
 * On failure the store is unchanged; scratch contents are unspecified. */
static inline int mulib_indexeddb_store_apply(mulib_indexeddb_store_t *s,
    const mulib_indexeddb_store_op *ops, size_t count,
    mulib_indexeddb_store_record *scratch, size_t scratch_cap)
{
    if (!mulib_indexeddb_store_shape_(s) || (count && !ops) ||
        count > SIZE_MAX / sizeof *ops)
        return 0;
    if (!count)
        return 1;
    if (scratch_cap < s->cap || (s->cap && (!scratch || scratch == s->v)))
        return 0;
    mulib_indexeddb_store_t staged = {scratch, s->len, s->cap};
    if (s->len)
        memcpy(scratch, s->v, s->len * sizeof *s->v);
    for (size_t i = 0; i < count; i++) {
        int ok;
        switch (ops[i].kind) {
        case mulib_indexeddb_store_ADD:
        case mulib_indexeddb_store_PUT:
            ok = mulib_indexeddb_store_put(&staged, ops[i].key, ops[i].value,
                ops[i].kind == mulib_indexeddb_store_PUT);
            break;
        case mulib_indexeddb_store_DELETE:
            ok = mulib_indexeddb_store_delete(&staged, ops[i].key);
            break;
        default:
            return 0;
        }
        if (!ok)
            return 0;
    }
    if (staged.len)
        memcpy(s->v, scratch, staged.len * sizeof *s->v);
    s->len = staged.len;
    return 1;
}
#endif /* MULIB_INDEXEDDB_STORE_H */
