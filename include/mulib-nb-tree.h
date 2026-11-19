/* SPDX-License-Identifier: MIT */
#ifndef MULIB_NB_TREE_H
#define MULIB_NB_TREE_H
#include "mulib-nb-core.h"

static inline int
mulib_nb_reparent(mulib_nb *n, uint64_t id, uint64_t parent)
{
    mulib_nb_cell *c = mulib_nb_find(n, id);
    if (!c)
        return 0;
    uint64_t p = parent;
    for (size_t depth = 0; p; ++depth) {
        mulib_nb_cell *q = mulib_nb_find(n, p);
        if (depth >= n->n || !q || p == id || q->kind != MULIB_NB_GROUP)
            return 0;
        p = q->parent;
    }
    if (c->parent == parent)
        return 1;
    if (n->revision == UINT64_MAX)
        return 0;
    c->parent = parent;
    ++n->revision;
    return 1;
}

static inline int
mulib_nb_set_collapsed(mulib_nb *n, uint64_t id, int collapsed)
{
    mulib_nb_cell *c = mulib_nb_find(n, id);
    if (!c)
        return 0;
    unsigned flags = collapsed ? c->flags | MULIB_NB_COLLAPSED
                               : c->flags & ~MULIB_NB_COLLAPSED;
    if (flags == c->flags)
        return 1;
    if (n->revision == UINT64_MAX)
        return 0;
    c->flags = flags;
    ++n->revision;
    return 1;
}

static inline int
mulib_nb_visible(mulib_nb *n, uint64_t id)
{
    mulib_nb_cell *c = mulib_nb_find(n, id);
    if (!c)
        return 0;
    for (size_t k = 0; c->parent; ++k) {
        if (k >= n->n)
            return 0;
        c = mulib_nb_find(n, c->parent);
        if (!c || c->kind != MULIB_NB_GROUP || (c->flags & MULIB_NB_COLLAPSED))
            return 0;
    }
    return 1;
}

/* Private ephemeral ID index: bounded open addressing, no document pointers. */
typedef struct { uint64_t id; size_t index; } mulib_nb_id_slot;

static inline size_t
mulib_nb_id_hash(uint64_t id, size_t mask)
{
    id ^= id >> 30;
    id *= UINT64_C(0xbf58476d1ce4e5b9);
    id ^= id >> 27;
    id *= UINT64_C(0x94d049bb133111eb);
    id ^= id >> 31;
    return (size_t)id & mask;
}

static inline size_t
mulib_nb_id_lookup(const mulib_nb_id_slot *slots, size_t cap, uint64_t id)
{
    if (!id || !slots || !cap)
        return SIZE_MAX;
    size_t at = mulib_nb_id_hash(id, cap - 1);
    while (slots[at].id) {
        if (slots[at].id == id)
            return slots[at].index;
        at = (at + 1) & (cap - 1);
    }
    return SIZE_MAX;
}

/* Validates IDs and the entire parent graph in expected O(n), including chains
 * in arbitrary document order. out is a malloc-owned byte per cell. */
static inline int
mulib_nb_tree_visibility(const mulib_nb *n, unsigned char **out)
{
    if (!n || !out || (n->n && !n->v) || n->n > SIZE_MAX / 2)
        return 0;
    size_t cap = 8;
    while (cap < n->n * 2) {
        if (cap > SIZE_MAX / 2)
            return 0;
        cap *= 2;
    }
    if (cap > SIZE_MAX / sizeof(mulib_nb_id_slot)
        || n->n > SIZE_MAX / sizeof(size_t))
        return 0;
    mulib_nb_id_slot *slots = (mulib_nb_id_slot *)calloc(cap, sizeof *slots);
    size_t *parents = (size_t *)malloc((n->n ? n->n : 1) * sizeof *parents);
    size_t *stack = (size_t *)malloc((n->n ? n->n : 1) * sizeof *stack);
    unsigned char *state = (unsigned char *)calloc(n->n ? n->n : 1, 1);
    unsigned char *visible = (unsigned char *)calloc(n->n ? n->n : 1, 1);
    int ok = 0;
    if (!slots || !parents || !stack || !state || !visible)
        goto done;
    for (size_t i = 0; i < n->n; ++i) {
        uint64_t id = n->v[i].id;
        if (!id || id > n->next)
            goto done;
        size_t at = mulib_nb_id_hash(id, cap - 1);
        while (slots[at].id) {
            if (slots[at].id == id)
                goto done;
            at = (at + 1) & (cap - 1);
        }
        slots[at] = (mulib_nb_id_slot){id, i};
    }
    for (size_t i = 0; i < n->n; ++i) {
        parents[i] = mulib_nb_id_lookup(slots, cap, n->v[i].parent);
        if (n->v[i].parent && (parents[i] == SIZE_MAX
            || n->v[parents[i]].kind != MULIB_NB_GROUP))
            goto done;
    }
    for (size_t i = 0; i < n->n; ++i) {
        size_t k = i, depth = 0;
        while (k != SIZE_MAX && !state[k]) {
            stack[depth++] = k;
            state[k] = 1;
            k = parents[k];
        }
        if (k != SIZE_MAX && state[k] == 1)
            goto done;
        while (depth) {
            k = stack[--depth];
            size_t p = parents[k];
            visible[k] = (unsigned char)(p == SIZE_MAX
                || (visible[p] && !(n->v[p].flags & MULIB_NB_COLLAPSED)));
            state[k] = 2;
        }
    }
    *out = visible;
    visible = NULL;
    ok = 1;
done:
    free(slots);
    free(parents);
    free(stack);
    free(state);
    free(visible);
    return ok;
}
#endif
