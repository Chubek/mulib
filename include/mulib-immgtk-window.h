/* Ordered windows: later equal-z windows occlude earlier windows, even in gaps. */
#ifndef MULIB_IMMGTK_WINDOW_H
#define MULIB_IMMGTK_WINDOW_H
#include "mulib-immgtk-native.h"
typedef struct {
    mgx_rect old_clip, content;
    int old_z, opened;
    mgx_rect resize;
    int draw_resize;
    size_t resize_item;
} mulib_immgtk_window_scope_t;
enum {
    MIG_WINDOW_RESIZABLE = 1,
    MIG_WINDOW_COLLAPSIBLE = 2,
    MIG_WINDOW_NO_MOVE = 4
};
/* IDs and optional state allocations are validated/reserved before consuming
 * actions, mutating bounds/collapse, registering items, or drawing anything. */
static inline int mig_window_reserve(mulib_immgtk_native_t *g,
        const uint64_t *ids, size_t n, const uint64_t *states, size_t sn, size_t draws) {
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < i; ++j) {
            if (ids[i] == ids[j]) {
                g->error |= MIG_ERROR_ID;
                return 0;
            }
        }
        for (size_t j = 0; j < g->n; ++j) {
            if (ids[i] == g->items[j].id) {
                g->error |= MIG_ERROR_ID;
                return 0;
            }
        }
    }
    size_t missing = 0;
    for (size_t i = 0; i < sn; ++i) {
        size_t j = 0;
        while (j < g->sn && g->states[j].id != states[i]) ++j;
        missing += j == g->sn;
    }
    if (g->n > SIZE_MAX - n || g->sn > SIZE_MAX - missing ||
        g->draws.n > SIZE_MAX - draws) {
        g->error |= MIG_ERROR_LIMIT;
        return 0;
    }
    return mig_reserve(g, (void **)&g->items, &g->cap, g->n + n,
               sizeof *g->items, g->max_items) &&
        mig_reserve(g, (void **)&g->states, &g->sc, g->sn + missing,
               sizeof *g->states, g->max_states) &&
        mig_reserve(g, (void **)&g->draws.v, &g->draws.cap, g->draws.n + draws,
               sizeof *g->draws.v, g->max_draws);
}
/* A drag state owns only its origin rectangle, never a borrowed widget pointer. */
static inline void mig_window_drag(mulib_immgtk_native_t *g, uint64_t id,
        mgx_rect handle, mgx_rect *bounds, int resize, int min_w, int min_h) {
    mig_state *st = mig_state_get(g, id);
    if (!st) return;
    int offset_x = handle.x - bounds->x, offset_y = handle.y - bounds->y;
    int eligible = g->n && g->items[g->n - 1].id == id &&
        mig_eligible(g, g->items + g->n - 1);
    for (size_t i = 0; i < g->an; ++i) {
        mig_action *a = g->actions + i;
        if (a->id != id || !(a->keyboard & MIG_ACTION_DRAG)) continue;
        if (!eligible || a->scope != g->scope) {
            st->flags = 0;
            a->id = 0;
            continue;
        }
        if (a->keyboard & MIG_DRAG_START) {
            handle.x = resize ? bounds->x + bounds->w - 12 : bounds->x + offset_x;
            handle.y = resize ? bounds->y + bounds->h - 12 : bounds->y + offset_y;
            if (!mgx_contains(handle, a->press_x, a->press_y) ||
                !mgx_contains(g->clip, a->press_x, a->press_y)) {
                st->flags = 0;
                if (g->active == id) g->active = 0;
                a->id = 0;
                continue;
            }
            st->rect = *bounds;
            st->flags = 1;
        }
        if (st->flags) {
            double dx = a->x - a->press_x, dy = a->y - a->press_y;
            if (resize) {
                /* Resize never changes the origin. Saturate before conversion. */
                bounds->w = mgx_int(mgx_clamp(st->rect.w + dx, min_w,
                    fmin(INT_MAX, (double)INT_MAX - bounds->x)));
                bounds->h = mgx_int(mgx_clamp(st->rect.h + dy, min_h,
                    fmin(INT_MAX, (double)INT_MAX - bounds->y)));
            } else {
                bounds->x = mgx_int(mgx_clamp(st->rect.x + dx, INT_MIN,
                    (double)INT_MAX - bounds->w));
                bounds->y = mgx_int(mgx_clamp(st->rect.y + dy, INT_MIN,
                    (double)INT_MAX - bounds->h));
            }
        }
        if (a->keyboard & MIG_DRAG_END) st->flags = 0;
        a->id = 0;
    }
    if (g->active != id || !g->down) st->flags = 0;
}
/* Success opens a scope even when collapsed. Check scope.content.h > 0 to
 * omit content, and always pair a successful begin with window_end.
 * bounds retains the expanded height while the collapsed frame is 24px high.
 * Paint/hit z order remains explicit submission order; no hidden draw replay. */
static inline int mulib_immgtk_window_begin_ex(mulib_immgtk_native_t *g,
        const char *title, mgx_rect *bounds, unsigned flags, int *collapsed,
        int min_w, int min_h, mulib_immgtk_window_scope_t *s) {
    if (!s) return 0;
    memset(s, 0, sizeof *s);
    if (!g || !g->in_frame || !title || !bounds ||
        flags & ~(MIG_WINDOW_RESIZABLE | MIG_WINDOW_COLLAPSIBLE | MIG_WINDOW_NO_MOVE) ||
        ((flags & MIG_WINDOW_COLLAPSIBLE) && !collapsed) ||
        min_w < 40 || min_h < 40 || bounds->w < min_w || bounds->h < min_h ||
        (int64_t)bounds->x + bounds->w > INT_MAX ||
        (int64_t)bounds->y + bounds->h > INT_MAX || g->z == INT_MAX ||
        g->error ||
        !mig_push_id(g, title)) return 0;
    uint64_t background_id = mig_id(g, "#background");
    uint64_t title_id = mig_id(g, "#title");
    uint64_t collapse_id = mig_id(g, "#collapse");
    uint64_t resize_id = mig_id(g, "#resize");
    int can_collapse = !!(flags & MIG_WINDOW_COLLAPSIBLE);
    int folded = can_collapse && !!*collapsed;
    int can_resize = !!(flags & MIG_WINDOW_RESIZABLE);
    uint64_t ids[4] = {background_id, title_id, 0, 0}, states[2];
    size_t count = 2, sn = 0;
    if (can_collapse) ids[count++] = collapse_id;
    if (can_resize) ids[count++] = resize_id;
    if (!(flags & MIG_WINDOW_NO_MOVE)) states[sn++] = title_id;
    if (can_resize) states[sn++] = resize_id;
    size_t draws = 2 + (can_collapse ? 2u : 0u) + (can_resize ? 1u : 0u);
    if (!mig_window_reserve(g, ids, count, states, sn, draws)) {
        (void)mig_pop_id(g);
        return 0;
    }
    s->old_clip = g->clip;
    s->old_z = g->z;
    s->opened = 1;
    ++g->z;
    size_t background = g->n;
    mgx_rect frame = *bounds;
    if (folded) frame.h = 24;
    (void)mig_block_region(g, background_id, frame);
    mgx_rect bar = {bounds->x + (can_collapse ? 24 : 0), bounds->y,
        bounds->w - (can_collapse ? 24 : 0), 24};
    size_t title_at = g->n;
    (void)mig_interact_mode(g, title_id, bar, 1, !(flags & MIG_WINDOW_NO_MOVE));
    if (!(flags & MIG_WINDOW_NO_MOVE))
        mig_window_drag(g, title_id, bar, bounds, 0, min_w, min_h);
    bar = (mgx_rect){bounds->x + (can_collapse ? 24 : 0), bounds->y,
        bounds->w - (can_collapse ? 24 : 0), 24};
    g->items[title_at].bounds = bar;
    if (can_collapse) {
        mgx_rect toggle = {bounds->x, bounds->y, 24, 24};
        if (mig_interact(g, collapse_id, toggle, 1)) {
            *collapsed = !folded;
            folded = !folded;
        }
    }
    if (can_resize && !folded) {
        mgx_rect handle = {bounds->x + bounds->w - 12, bounds->y + bounds->h - 12, 12, 12};
        size_t at = g->n;
        (void)mig_interact_mode(g, resize_id, handle, 1, 1);
        mig_window_drag(g, resize_id, handle, bounds, 1, min_w, min_h);
        s->resize = (mgx_rect){bounds->x + bounds->w - 12,
            bounds->y + bounds->h - 12, 12, 12};
        g->items[at].bounds = s->resize;
        s->draw_resize = 1;
        s->resize_item = at;
    }
    bar = (mgx_rect){bounds->x + (can_collapse ? 24 : 0), bounds->y,
        bounds->w - (can_collapse ? 24 : 0), 24};
    g->items[title_at].bounds = bar;
    frame = *bounds;
    if (folded) frame.h = 24;
    g->items[background].bounds = frame;
    mig_rect(g, frame, g->theme.panel);
    mgx_rect full_bar = {bounds->x, bounds->y, bounds->w, 24};
    mig_rect(g, full_bar, g->theme.active);
    if (can_collapse) {
        mgx_rect mark = {bounds->x + 8, bounds->y + 11, 8, 2};
        mig_rect(g, mark, g->theme.text);
        if (folded)
            mig_rect(g, (mgx_rect){bounds->x + 11, bounds->y + 8, 2, 8}, g->theme.text);
    }
    mig_label(g, bar, title);
    s->content = folded ? (mgx_rect){bounds->x, bounds->y + 24, bounds->w, 0} :
        mgui_inset((mgx_rect){bounds->x, bounds->y + 24,
            bounds->w, bounds->h - 24}, g->theme.padding);
    g->clip = mgx_intersect(g->clip, s->content);
    return 1;
}
static inline int mulib_immgtk_window_begin(mulib_immgtk_native_t *g, const char *title,
        mgx_rect *bounds, mulib_immgtk_window_scope_t *s) {
    return mulib_immgtk_window_begin_ex(g, title, bounds, 0, NULL, 40, 40, s);
}
static inline void mulib_immgtk_window_end(mulib_immgtk_native_t *g,
        mulib_immgtk_window_scope_t *s) {
    if (g && s && s->opened) {
        g->clip = s->old_clip;
        if (s->draw_resize) {
            /* Put chrome after its own content, not above the next window's z.
             * Item indices, unlike pointers, survive child allocation growth. */
            size_t at = s->resize_item;
            mig_item grip = g->items[at];
            memmove(g->items + at, g->items + at + 1, (g->n - at - 1) * sizeof grip);
            g->items[g->n - 1] = grip;
            mig_rect(g, s->resize, g->theme.accent);
        }
        g->z = s->old_z;
        (void)mig_pop_id(g);
        s->opened = 0;
    }
}
#endif
