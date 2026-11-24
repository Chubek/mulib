/* Popup stack plus explicit, full-surface input blocking. */
#ifndef MULIB_IMMGTK_POPUP_H
#define MULIB_IMMGTK_POPUP_H
#include "mulib-immgtk-native.h"
typedef struct {
    uint64_t ids[32];
    size_t n;
    uint64_t prepared, restore_focus;
} mulib_immgtk_popup_t;
typedef struct {
    mgx_rect old_clip;
    uint64_t old_scope;
    int old_z, opened;
} mulib_immgtk_popup_scope_t;
static inline int mulib_immgtk_popup_open(mulib_immgtk_popup_t *p, uint64_t id) {
    if (!p || !id || p->n > 32) return 0;
    for (size_t i = 0; i < p->n; ++i) {
        if (p->ids[i] == id) { p->n = i + 1; return 1; }
    }
    if (p->n == 32) return 0;
    p->ids[p->n++] = id;
    return 1;
}
static inline int mulib_immgtk_popup_is_open(const mulib_immgtk_popup_t *p, uint64_t id) {
    if (!p || p->n > 32) return 0;
    for (size_t i = 0; i < p->n; ++i) if (p->ids[i] == id) return 1;
    return 0;
}
static inline void mulib_immgtk_popup_close(mulib_immgtk_popup_t *p) {
    if (p && p->n && p->n <= 32) --p->n;
}
/* Call before begin/input and whenever open/close changes during a frame. */
static inline void mulib_immgtk_popup_prepare(mulib_immgtk_native_t *g,
        mulib_immgtk_popup_t *p) {
    if (!g || !p || p->n > 32) return;
    uint64_t top = p->n ? p->ids[p->n - 1] : 0;
    if (top == p->prepared) return;
    if (!p->prepared) p->restore_focus = g->focus;
    mulib_immgtk_native_block(g, top);
    if (!top) {
        const mig_item *item = mig_previous(g, p->restore_focus);
        if (item && mig_eligible(g, item)) g->focus = item->id;
        p->restore_focus = 0;
    }
    p->prepared = top;
}
/* Begin/end scopes collect popup geometry. Top popup alone receives input.
 * Use the same stack/context pair throughout its lifetime; do not copy it. */
static inline int mulib_immgtk_popup_begin(mulib_immgtk_native_t *g,
        mulib_immgtk_popup_t *p, uint64_t id, mgx_rect bounds,
        mulib_immgtk_popup_scope_t *s) {
    if (!s) return 0;
    memset(s, 0, sizeof *s);
    if (!g || !g->in_frame || !mulib_immgtk_popup_is_open(p, id) ||
        bounds.w <= 0 || bounds.h <= 0) return 0;
    mulib_immgtk_popup_prepare(g, p);
    s->old_clip = g->clip;
    s->old_scope = g->scope;
    s->old_z = g->z;
    s->opened = 1;
    g->scope = id;
    g->z = INT_MAX - 32 + (int)p->n;
    g->clip = mgx_intersect(g->clip, bounds);
    return 1;
}
static inline void mulib_immgtk_popup_end(mulib_immgtk_native_t *g,
        mulib_immgtk_popup_scope_t *s) {
    if (!g || !s || !s->opened) return;
    g->clip = s->old_clip;
    g->scope = s->old_scope;
    g->z = s->old_z;
    s->opened = 0;
}
/* Optional dismissal, after begin/input and before any widgets. A modal can
 * simply omit this helper. Dismissal never activates a background item. */
static inline int mulib_immgtk_popup_dismiss(mulib_immgtk_native_t *g,
        mulib_immgtk_popup_t *p, mgx_rect bounds, int outside, int escape) {
    if (!g || !p || !p->n || !g->in_frame) return 0;
    if ((escape && g->escape) ||
        (outside && g->pressed && !mgx_contains(bounds, g->press_x, g->press_y))) {
        mulib_immgtk_popup_close(p);
        mulib_immgtk_popup_prepare(g, p);
        return 1;
    }
    return 0;
}
#endif
