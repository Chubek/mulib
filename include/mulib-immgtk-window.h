/* Ordered windows: later equal-z windows occlude earlier windows, even in gaps. */
#ifndef MULIB_IMMGTK_WINDOW_H
#define MULIB_IMMGTK_WINDOW_H
#include "mulib-immgtk-native.h"
typedef struct {
    mgx_rect old_clip, content;
    int old_z, opened;
} mulib_immgtk_window_scope_t;
static inline int mulib_immgtk_window_begin(mulib_immgtk_native_t *g, const char *title,
        mgx_rect *bounds, mulib_immgtk_window_scope_t *s) {
    if (!s) return 0;
    memset(s, 0, sizeof *s);
    if (!g || !g->in_frame || !bounds || bounds->w < 40 || bounds->h < 40 ||
        (int64_t)bounds->x + bounds->w > INT_MAX ||
        (int64_t)bounds->y + bounds->h > INT_MAX || g->z == INT_MAX ||
        !mig_push_id(g, title)) return 0;
    *s = (mulib_immgtk_window_scope_t){g->clip, *bounds, g->z, 1};
    ++g->z;
    size_t background = g->n;
    int have_background = mig_block_region(g, mig_id(g, "#background"), *bounds);
    uint64_t id = mig_id(g, "#title");
    mgx_rect bar = {bounds->x, bounds->y, bounds->w, 24};
    size_t title_at = g->n;
    (void)mig_interact(g, id, bar, 1);
    int have_title = title_at < g->n;
    mig_state *st = mig_state_get(g, id);
    if (st) {
        if (g->pressed && g->active == id) {
            st->rect.x = mgx_int(g->press_x - bounds->x);
            st->rect.y = mgx_int(g->press_y - bounds->y);
        }
        if (g->down && g->active == id) {
            bounds->x = mgx_int(mgx_clamp(g->x - st->rect.x, INT_MIN, INT_MAX - bounds->w));
            bounds->y = mgx_int(mgx_clamp(g->y - st->rect.y, INT_MIN, INT_MAX - bounds->h));
        }
    }
    if (have_background) g->items[background].bounds = *bounds;
    bar = (mgx_rect){bounds->x, bounds->y, bounds->w, 24};
    if (have_title) g->items[title_at].bounds = bar;
    mig_rect(g, *bounds, g->theme.panel);
    mig_rect(g, bar, g->theme.active);
    mig_label(g, bar, title);
    s->content = mgui_inset((mgx_rect){bounds->x, bounds->y + 24,
        bounds->w, bounds->h - 24}, g->theme.padding);
    g->clip = mgx_intersect(g->clip, s->content);
    return 1;
}
static inline void mulib_immgtk_window_end(mulib_immgtk_native_t *g,
        mulib_immgtk_window_scope_t *s) {
    if (g && s && s->opened) {
        g->clip = s->old_clip;
        g->z = s->old_z;
        (void)mig_pop_id(g);
        s->opened = 0;
    }
}
#endif
