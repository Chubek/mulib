/* MuLIB graphical extension: gui-undo. C11, header-only. */
#ifndef MULIB_GUI_UNDO_H
#define MULIB_GUI_UNDO_H
#include "mulib-gui-core.h"
/* Callbacks must be failure-atomic themselves. A failed push does not transfer
 * ownership of user; successful pushes release it on branch discard or free.
 * Callback-driven stack mutations are rejected, including free during a callback. */
typedef struct {
    int (*apply)(void *), (*revert)(void *);
    void (*release)(void *);
    void *user;
} mgui_undo_cmd;
typedef mgui_undo_cmd mulib_gui_undo_cmd_t;

typedef struct {
    mgui_undo_cmd *v;
    size_t n, cap, pos;
    int busy;
} mulib_gui_undo_t;

static inline int mgui_undo_storage(const mulib_gui_undo_t *u)
{
    return u && !u->busy && u->pos <= u->n && u->n <= u->cap &&
        u->cap <= SIZE_MAX / sizeof *u->v &&
        (u->v ? u->cap != 0 : u->cap == 0);
}

static inline int mulib_gui_undo_push(mulib_gui_undo_t *u, mgui_undo_cmd c)
{
    mgui_undo_cmd *v;
    size_t cap, need;
    if (!mgui_undo_storage(u) || !c.apply || !c.revert ||
        u->pos == SIZE_MAX / sizeof *u->v) return 0;
    need = u->pos + 1;
    cap = u->cap;
    v = u->v;
    if (need > cap) {
        /* Do not realloc the live vector before a potentially failing apply. */
        v = NULL;
        cap = 0;
        if (!mgx_grow((void **)&v, &cap, need, sizeof *v)) return 0;
        if (u->pos) memcpy(v, u->v, u->pos * sizeof *v);
    }
    u->busy = 1;
    if (!c.apply(c.user)) {
        u->busy = 0;
        if (v != u->v) free(v);
        return 0;
    }
    for (size_t i = u->pos; i < u->n; i++)
        if (u->v[i].release) u->v[i].release(u->v[i].user);
    if (v != u->v) free(u->v);
    u->v = v;
    u->cap = cap;
    u->n = need;
    u->v[u->pos] = c;
    u->pos = need;
    u->busy = 0;
    return 1;
}

static inline int mulib_gui_undo_step(mulib_gui_undo_t *u, int redo)
{
    size_t p;
    int ok;
    if (!mgui_undo_storage(u) || (redo ? u->pos == u->n : u->pos == 0))
        return 0;
    p = redo ? u->pos : u->pos - 1;
    u->busy = 1;
    ok = redo ? u->v[p].apply(u->v[p].user) : u->v[p].revert(u->v[p].user);
    u->busy = 0;
    if (!ok) return 0;
    u->pos = redo ? p + 1 : p;
    return 1;
}

static inline void mulib_gui_undo_free(mulib_gui_undo_t *u)
{
    if (!u || u->busy) return;
    u->busy = 1;
    for (size_t i = 0; i < u->n; i++)
        if (u->v[i].release) u->v[i].release(u->v[i].user);
    free(u->v);
    memset(u, 0, sizeof *u);
}
#endif
