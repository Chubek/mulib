/* SPDX-License-Identifier: MIT */
#ifndef MULIB_NB_ORDER_H
#define MULIB_NB_ORDER_H
#include "mulib-nb-core.h"

/* to is the final index, not an insertion boundary. */
static inline int
mulib_nb_move(mulib_nb *n, uint64_t id, size_t to)
{
    mulib_nb_cell *c = mulib_nb_find(n, id);
    if (!c || to >= n->n)
        return 0;
    size_t from = (size_t)(c - n->v);
    if (from == to)
        return 1;
    if (n->revision == UINT64_MAX)
        return 0;
    mulib_nb_cell value = *c;
    if (from < to)
        memmove(c, c + 1, (to - from) * sizeof value);
    else
        memmove(n->v + to + 1, n->v + to, (from - to) * sizeof value);
    n->v[to] = value;
    ++n->revision;
    return 1;
}
#endif
