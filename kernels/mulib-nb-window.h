/* SPDX-License-Identifier: MIT */
#ifndef MULIB_NB_WINDOW_H
#define MULIB_NB_WINDOW_H
#include "mulib-nb-core.h"

/* Non-owning, single-active-document association. Detach before cell removal.
 * The host chooses hide/close policy and reconciles peers after history replay. */
static inline int
mulib_nb_window_attach(mulib_nb *n, mulib_wm_core *wm,
                       uint64_t cell, uint64_t window)
{
    mulib_nb_cell *c = mulib_nb_find(n, cell);
    mulib_wm_window *w = mulib_wm_find(wm, window);
    if (!c || !w || c->kind != MULIB_NB_APP
        || (c->window && c->window != window) || (w->cell && w->cell != cell))
        return 0;
    if (c->window == window && w->cell == cell)
        return 1;
    if (n->revision == UINT64_MAX || wm->revision == UINT64_MAX)
        return 0;
    c->window = window;
    w->cell = cell;
    ++n->revision;
    ++wm->revision;
    return 1;
}

static inline int
mulib_nb_window_detach(mulib_nb *n, mulib_wm_core *wm, uint64_t cell)
{
    mulib_nb_cell *c = mulib_nb_find(n, cell);
    if (!c || !c->window || n->revision == UINT64_MAX)
        return 0;
    mulib_wm_window *w = mulib_wm_find(wm, c->window);
    if (w && w->cell == cell) {
        if (wm->revision == UINT64_MAX)
            return 0;
        w->cell = 0;
        ++wm->revision;
    }
    c->window = 0;
    ++n->revision;
    return 1;
}
#endif
