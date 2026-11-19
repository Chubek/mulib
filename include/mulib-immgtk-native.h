/* Native immediate-mode UI. C11; contexts and borrowed resources are thread-owned. */
#ifndef MULIB_IMMGTK_NATIVE_H
#define MULIB_IMMGTK_NATIVE_H
#include "mulib-gui-core.h"

enum {
    MIG_ERROR_LIFETIME = 1, MIG_ERROR_LIMIT = 2, MIG_ERROR_MEMORY = 4,
    MIG_ERROR_ID = 8, MIG_ERROR_SCOPE = 16, MIG_ERROR_INPUT = 32
};
typedef struct {
    mgx_rect bounds, clip;
    uint64_t id;
    int z, enabled;
    uint64_t scope;
    int passive;
} mig_item;
typedef struct {
    uint64_t id;
    double value;
    int flags;
    uint64_t seen;
    mgx_rect rect;
} mig_state;
typedef struct {
    uint64_t id, scope;
    double press_x, press_y, x, y;
    int keyboard;
} mig_action;
typedef struct {
    mgx_surface *surface;
    const mulib_glx_font_t *font;
    mgui_theme theme;
    mulib_glx_drawlist_t draws;
    mgx_rect clip;
    uint64_t frame, hot, active, focus;
    double x, y, scroll;
    int down, pressed, released, activate;
    int z;
    uint64_t ids[64];
    size_t depth;
    mig_item *items, *previous;
    size_t n, cap, pn, pc;
    mig_state *states;
    size_t sn, sc;
    int error;
    /* Appended fields: existing public member names remain available. */
    mig_action *actions;
    size_t an, ac, max_items, max_states, max_actions, max_draws;
    uint64_t state_ttl, scope, blocker, capture_scope;
    double press_x, press_y;
    mgx_rect clips[64];
    size_t clip_depth;
    int in_frame, escape, unfocused;
    mig_action last_action;
} mulib_immgtk_native_t;
typedef mig_item mulib_immgtk_item_t;
typedef mig_state mulib_immgtk_state_t;

static inline void mulib_immgtk_native_init(mulib_immgtk_native_t *g) {
    if (!g) return;
    memset(g, 0, sizeof *g);
    g->theme = mgui_default_theme();
    g->max_items = g->max_states = g->max_draws = 65536;
    g->max_actions = 4096;
    g->state_ttl = 600;
}
/* Limits bound allocation capacity too. Set before building the first frame. */
static inline int mulib_immgtk_native_limits(mulib_immgtk_native_t *g,
        size_t items, size_t states, size_t actions, size_t draws) {
    if (!g || g->in_frame || g->cap || g->pc || g->sc || g->ac || g->draws.cap ||
        !items || !states || !actions || !draws) return 0;
    g->max_items = items;
    g->max_states = states;
    g->max_actions = actions;
    g->max_draws = draws;
    return 1;
}
static inline int mig_reserve(mulib_immgtk_native_t *g, void **p, size_t *cap,
        size_t need, size_t elem, size_t limit) {
    if (need > limit || need > SIZE_MAX / elem) {
        g->error |= MIG_ERROR_LIMIT;
        return 0;
    }
    if (need <= *cap) return 1;
    size_t next = *cap ? *cap : (limit < 8 ? limit : 8);
    while (next < need) next = next > limit / 2 ? limit : next * 2;
    if (next > SIZE_MAX / elem) next = need;
    void *v = realloc(*p, next * elem);
    if (!v) { g->error |= MIG_ERROR_MEMORY; return 0; }
    *p = v;
    *cap = next;
    return 1;
}
static inline uint64_t mig_id(const mulib_immgtk_native_t *g, const char *s) {
    if (!g || !s || g->depth > 64) return 0;
    uint64_t h = mgx_hash(s, strlen(s), g->depth ? g->ids[g->depth - 1] : 0);
    /* Delimit components: ["ab","c"] must not alias ["a","bc"]. */
    const unsigned char end = 0;
    return mgx_hash(&end, 1, h);
}
static inline int mig_push_id(mulib_immgtk_native_t *g, const char *s) {
    if (!g) return 0;
    if (!s || g->depth >= 64) { g->error |= MIG_ERROR_ID; return 0; }
    uint64_t id = mig_id(g, s);
    g->ids[g->depth++] = id;
    return 1;
}
static inline int mig_pop_id(mulib_immgtk_native_t *g) {
    if (!g) return 0;
    if (!g->depth) { g->error |= MIG_ERROR_SCOPE; return 0; }
    --g->depth;
    return 1;
}
static inline int mulib_immgtk_clip_push(mulib_immgtk_native_t *g, mgx_rect clip) {
    if (!g || !g->in_frame) return 0;
    if (g->clip_depth == 64) { g->error |= MIG_ERROR_SCOPE; return 0; }
    g->clips[g->clip_depth++] = g->clip;
    g->clip = mgx_intersect(g->clip, clip);
    return 1;
}
static inline int mulib_immgtk_clip_pop(mulib_immgtk_native_t *g) {
    if (!g || !g->in_frame) return 0;
    if (!g->clip_depth) { g->error |= MIG_ERROR_SCOPE; return 0; }
    g->clip = g->clips[--g->clip_depth];
    return 1;
}
static inline int mig_eligible(const mulib_immgtk_native_t *g, const mig_item *p) {
    mgx_rect visible = mgx_intersect(p->bounds, p->clip);
    if (g->surface) visible = mgx_intersect(visible, mgx_intersect(g->surface->clip,
        (mgx_rect){0, 0, g->surface->w, g->surface->h}));
    return p->enabled && (!g->blocker || p->scope == g->blocker) &&
        visible.w > 0 && visible.h > 0;
}
static inline const mig_item *mig_previous(const mulib_immgtk_native_t *g, uint64_t id) {
    for (size_t i = 0; i < g->pn; ++i)
        if (g->previous[i].id == id) return g->previous + i;
    return NULL;
}
static inline uint64_t mig_hit(const mulib_immgtk_native_t *g, double x, double y) {
    if (g->surface && (!mgx_contains(g->surface->clip, x, y) ||
        !mgx_contains((mgx_rect){0, 0, g->surface->w, g->surface->h}, x, y))) return 0;
    uint64_t id = 0;
    int z = INT_MIN;
    for (size_t i = 0; i < g->pn; ++i) {
        const mig_item *p = g->previous + i;
        if (mig_eligible(g, p) && p->z >= z &&
            mgx_contains(p->bounds, x, y) && mgx_contains(p->clip, x, y)) {
            id = p->id;
            z = p->z;
        }
    }
    return id;
}
static inline uint64_t mig_nav_step(mulib_immgtk_native_t *g, int reverse) {
    if (!g || !g->pn) return 0;
    size_t at = 0;
    while (at < g->pn && g->previous[at].id != g->focus) ++at;
    if (at == g->pn) at = reverse ? 0 : g->pn - 1;
    for (size_t i = 0; i < g->pn; ++i) {
        at = reverse ? (at ? at - 1 : g->pn - 1) : (at + 1 == g->pn ? 0 : at + 1);
        if (mig_eligible(g, g->previous + at) && !g->previous[at].passive)
            return g->focus = g->previous[at].id;
    }
    return g->focus = 0;
}
/* Before input: discard routing after structural layout changes. No retargeting. */
static inline void mulib_immgtk_native_invalidate(mulib_immgtk_native_t *g) {
    if (!g) return;
    g->pn = g->an = 0;
    g->hot = g->active = g->focus = 0;
}
/* A nonzero scope blocks ALL other scopes, including empty popup background. */
static inline void mulib_immgtk_native_block(mulib_immgtk_native_t *g, uint64_t scope) {
    if (!g || g->blocker == scope) return;
    g->blocker = scope;
    g->an = 0; /* Never pass a dismissing/opening click through to another scope. */
    const mig_item *p = mig_previous(g, g->active);
    if (!p || !mig_eligible(g, p) || p->scope != g->capture_scope) g->active = 0;
    p = mig_previous(g, g->focus);
    if (!p || !mig_eligible(g, p)) g->focus = 0;
    g->hot = mig_hit(g, g->x, g->y);
}
static inline int mig_start(mulib_immgtk_native_t *g, mgx_surface *s) {
    if (!g) return 0;
    if (g->in_frame) { g->error |= MIG_ERROR_LIFETIME; return 0; }
    g->in_frame = 1;
    g->surface = s;
    ++g->frame;
    g->error = 0;
    g->n = g->an = g->depth = g->clip_depth = 0;
    g->scope = 0;
    g->z = g->pressed = g->released = g->activate = g->escape = 0;
    g->scroll = 0;
    memset(&g->last_action, 0, sizeof g->last_action);
    g->clip = s ? mgx_intersect(s->clip, (mgx_rect){0, 0, s->w, s->h}) :
        (mgx_rect){0, 0, 0, 0};
    const mig_item *p = mig_previous(g, g->active);
    if (!p || !mig_eligible(g, p) || p->scope != g->capture_scope) g->active = 0;
    p = mig_previous(g, g->focus);
    if (!p || !mig_eligible(g, p)) g->focus = 0;
    mulib_glx_drawlist_reset(&g->draws);
    return 1;
}
static inline int mig_action_add(mulib_immgtk_native_t *g, uint64_t id, int keyboard) {
    const mig_item *p = mig_previous(g, id);
    if (!id || !p || !mig_eligible(g, p)) return 1;
    if (!mig_reserve(g, (void **)&g->actions, &g->ac, g->an + 1,
            sizeof *g->actions, g->max_actions)) {
        g->an = 0;
        g->active = 0;
        return 0;
    }
    g->actions[g->an++] = (mig_action){id, p->scope, g->press_x, g->press_y,
        g->x, g->y, keyboard};
    return 1;
}
static inline void mig_pointer(mulib_immgtk_native_t *g, int down) {
    down = !!down;
    if (down == g->down) return;
    if (down) {
        g->pressed = 1;
        g->active = g->focus = mig_hit(g, g->x, g->y);
        const mig_item *p = mig_previous(g, g->active);
        if (p && p->passive) g->active = g->focus = 0;
        g->capture_scope = p ? p->scope : 0;
        g->press_x = g->x;
        g->press_y = g->y;
    } else {
        g->released = 1;
        if (g->active && g->active == mig_hit(g, g->x, g->y))
            (void)mig_action_add(g, g->active, 0);
    }
    g->down = down;
}
static inline void mulib_immgtk_native_begin(mulib_immgtk_native_t *g, mgx_surface *s,
        double x, double y, int down, int activate) {
    if (!mig_start(g, s)) return;
    if (!isfinite(x) || !isfinite(y)) {
        g->error |= MIG_ERROR_INPUT;
        g->active = g->focus = g->hot = 0;
        g->down = 0;
        return;
    }
    g->x = x;
    g->y = y;
    g->unfocused = 0;
    g->hot = mig_hit(g, x, y);
    mig_pointer(g, down);
    g->activate = !!activate;
    if (activate) (void)mig_action_add(g, g->focus, 1);
}
/* Events use normalized button 0 and Linux/evdev key codes. Ordered, not snapshots.
 * Button coordinates come from preceding MOTION events; x/y in BUTTON are ignored.
 * The first frame only registers geometry. Returns 0 on rejected/lost input. */
static inline int mulib_immgtk_native_begin_events(mulib_immgtk_native_t *g,
        mgx_surface *s, const mgx_event *events, size_t count) {
    if (!mig_start(g, s)) return 0;
    if ((!events && count) || count > g->max_actions) {
        g->error |= MIG_ERROR_INPUT;
        if (count > g->max_actions) g->error |= MIG_ERROR_LIMIT;
        g->active = g->focus = g->hot = 0;
        g->down = 0;
        return 0;
    }
    int focused = !g->unfocused;
    for (size_t i = 0; i < count; ++i) {
        const mgx_event *e = events + i;
        if (e->type == MGX_EVENT_FOCUS) {
            if (e->down != 0 && e->down != 1) { g->error |= MIG_ERROR_INPUT; break; }
            focused = !!e->down;
            g->unfocused = !focused;
            if (!focused) {
                g->active = g->focus = 0;
                g->down = 0;
                g->an = 0;
            }
        } else if (!focused) {
            continue;
        } else if (e->type == MGX_EVENT_MOTION) {
            if (!isfinite(e->x) || !isfinite(e->y)) {
                g->error |= MIG_ERROR_INPUT;
                break;
            }
            g->x = e->x;
            g->y = e->y;
        } else if (e->type == MGX_EVENT_BUTTON && e->code == 0) {
            if (e->down != 0 && e->down != 1) { g->error |= MIG_ERROR_INPUT; break; }
            mig_pointer(g, e->down);
        } else if (e->type == MGX_EVENT_SCROLL) {
            if (!isfinite(e->dx) || !isfinite(e->dy) || !isfinite(g->scroll + e->dy)) {
                g->error |= MIG_ERROR_INPUT;
                break;
            }
            g->scroll += e->dy;
        } else if (e->type == MGX_EVENT_KEY) {
            if (e->code >= 768 || e->down < 0 || e->down > 2) {
                g->error |= MIG_ERROR_INPUT;
                break;
            }
            if (e->down != 1) continue;
            if (e->code == 15) (void)mig_nav_step(g, !!(e->mods & 1));
            else if (e->code == 28 || e->code == 57) {
                g->activate = 1;
                (void)mig_action_add(g, g->focus, 1);
            } else if (e->code == 1) {
                g->escape = 1;
                g->active = 0;
            }
        }
        if (g->error) break;
    }
    if (g->error) {
        g->an = 0;
        g->active = g->focus = 0;
        g->down = 0;
    }
    g->hot = focused ? mig_hit(g, g->x, g->y) : 0;
    return !g->error;
}
/* Returned pointers are invalidated by the next state insertion or end/free. */
static inline mig_state *mig_state_get(mulib_immgtk_native_t *g, uint64_t id) {
    if (!g || !id) return NULL;
    for (size_t i = 0; i < g->sn; ++i) {
        if (g->states[i].id == id) {
            g->states[i].seen = g->frame;
            return g->states + i;
        }
    }
    if (!mig_reserve(g, (void **)&g->states, &g->sc, g->sn + 1,
            sizeof *g->states, g->max_states)) return NULL;
    mig_state *p = g->states + g->sn++;
    *p = (mig_state){id, 0, 0, g->frame, {0, 0, 0, 0}};
    return p;
}
static inline int mig_interact(mulib_immgtk_native_t *g, uint64_t id,
        mgx_rect r, int enabled) {
    if (!g || !g->in_frame) {
        if (g) g->error |= MIG_ERROR_LIFETIME;
        return 0;
    }
    memset(&g->last_action, 0, sizeof g->last_action);
    if (!id) { g->error |= MIG_ERROR_ID; return 0; }
    if (r.w <= 0 || r.h <= 0) return 0;
    for (size_t i = 0; i < g->n; ++i) {
        if (g->items[i].id == id) { g->error |= MIG_ERROR_ID; return 0; }
    }
    if (!mig_reserve(g, (void **)&g->items, &g->cap, g->n + 1,
            sizeof *g->items, g->max_items)) return 0;
    mgx_rect clip = g->surface ? mgx_intersect(g->clip,
        mgx_intersect(g->surface->clip, (mgx_rect){0, 0, g->surface->w, g->surface->h})) :
        g->clip;
    mig_item *p = g->items + g->n++;
    *p = (mig_item){r, clip, id, g->z, !!enabled, g->scope, 0};
    int eligible = mig_eligible(g, p);
    if (g->focus == id && !eligible) g->focus = 0;
    if (g->active == id && (!eligible || g->capture_scope != g->scope ||
        !mgx_contains(r, g->press_x, g->press_y) ||
        !mgx_contains(clip, g->press_x, g->press_y))) g->active = 0;
    int clicked = 0;
    for (size_t i = 0; i < g->an; ++i) {
        mig_action *a = g->actions + i;
        if (a->id != id) continue;
        if (eligible && a->scope == g->scope &&
            (a->keyboard || (mgx_contains(r, a->press_x, a->press_y) &&
                mgx_contains(clip, a->press_x, a->press_y) &&
                mgx_contains(r, a->x, a->y) && mgx_contains(clip, a->x, a->y)))) {
            clicked = 1;
            g->last_action = *a;
        }
        a->id = 0; /* An immediate-mode boolean coalesces repeated clicks per item. */
    }
    return clicked;
}
/* Blocks a lower-z hit without becoming a focus/navigation target. */
static inline int mig_block_region(mulib_immgtk_native_t *g, uint64_t id, mgx_rect r) {
    size_t before = g ? g->n : 0;
    (void)mig_interact(g, id, r, 1);
    if (!g || before == g->n) return 0;
    g->items[g->n - 1].passive = 1;
    return 1;
}
static inline int mig_draw_add(mulib_immgtk_native_t *g, mgx_drawcmd c) {
    if (!g || !g->in_frame) {
        if (g) g->error |= MIG_ERROR_LIFETIME;
        return 0;
    }
    if (!mig_reserve(g, (void **)&g->draws.v, &g->draws.cap, g->draws.n + 1,
            sizeof *g->draws.v, g->max_draws)) return 0;
    g->draws.v[g->draws.n++] = c;
    return 1;
}
static inline void mig_rect(mulib_immgtk_native_t *g, mgx_rect r, uint32_t color) {
    if (!g) return;
    mgx_drawcmd c = {0};
    c.kind = MGX_RECT;
    c.bounds = r;
    c.clip = g->clip;
    c.color = color;
    (void)mig_draw_add(g, c);
}
static inline void mig_flush(mulib_immgtk_native_t *g) {
    if (!g) return;
    if (g->surface) mulib_glx_drawlist_render(&g->draws, g->surface);
    mulib_glx_drawlist_reset(&g->draws);
}
static inline void mig_label(mulib_immgtk_native_t *g, mgx_rect r, const char *label) {
    if (!g || !g->in_frame || !g->surface || !g->font || !label) return;
    mig_flush(g); /* Preserve label/primitive order; font and label are borrowed here. */
    mgx_rect old = g->surface->clip;
    g->surface->clip = mgx_intersect(old, mgx_intersect(g->clip, r));
    mulib_glx_text_draw(g->surface, g->font, mgx_int(r.x + (double)g->theme.padding),
        mgx_int(r.y + fmax(0, ((double)r.h - g->theme.font_size) / 2)), label, strlen(label),
        g->theme.font_size, g->theme.text);
    g->surface->clip = old;
}
static inline int mig_button(mulib_immgtk_native_t *g, const char *id,
        const char *label, mgx_rect r, int enabled) {
    if (!g) return 0;
    uint64_t k = mig_id(g, id);
    int clicked = mig_interact(g, k, r, enabled);
    mig_rect(g, r, !enabled ? g->theme.panel : g->active == k && g->down ?
        g->theme.active : g->hot == k ? g->theme.hover : g->theme.normal);
    mig_label(g, r, label);
    return clicked;
}
static inline int mig_checkbox(mulib_immgtk_native_t *g, const char *id,
        const char *label, mgx_rect r, int *value) {
    if (!g || !value || r.w <= 0 || r.h <= 0) return 0;
    int c = mig_button(g, id, label, r, 1);
    if (c) *value = !*value;
    if (*value) mig_rect(g, (mgx_rect){mgx_int(r.x + (double)r.w - 18),
        mgx_int(r.y + 4.0), 14, mgx_max(0, r.h - 8)}, g->theme.accent);
    return c;
}
static inline int mig_slider(mulib_immgtk_native_t *g, const char *id,
        mgx_rect r, double *v, double lo, double hi) {
    if (!g || !g->in_frame || !id || !v || !isfinite(lo) || !isfinite(hi) || !isfinite(hi - lo) ||
        hi <= lo || r.w <= 0 || r.h <= 0) return 0;
    uint64_t k = mig_id(g, id);
    int clicked = mig_interact(g, k, r, 1);
    double before = *v;
    *v = isfinite(*v) ? mgx_clamp(*v, lo, hi) : lo;
    if (g->active == k && g->down)
        *v = lo + (hi - lo) * mgx_clamp((g->x - r.x) / r.w, 0, 1);
    else if (clicked && !g->last_action.keyboard)
        *v = lo + (hi - lo) * mgx_clamp((g->last_action.x - r.x) / r.w, 0, 1);
    mig_rect(g, r, g->theme.normal);
    mig_rect(g, (mgx_rect){r.x, r.y, mgx_int(r.w * ((*v - lo) / (hi - lo))), r.h},
        g->theme.accent);
    return before != *v;
}
static inline void mulib_immgtk_native_end(mulib_immgtk_native_t *g) {
    if (!g) return;
    if (!g->in_frame) { g->error |= MIG_ERROR_LIFETIME; return; }
    mig_flush(g);
    if (g->depth || g->clip_depth || g->scope) g->error |= MIG_ERROR_SCOPE;
    int f = 0, a = 0;
    for (size_t i = 0; i < g->n; ++i) {
        if (mig_eligible(g, g->items + i)) {
            f |= g->items[i].id == g->focus;
            a |= g->items[i].id == g->active;
        }
    }
    if (!f) g->focus = 0;
    if (!a || !g->down) g->active = 0;
    mig_item *v = g->previous;
    size_t c = g->pc;
    g->previous = g->items;
    g->pn = g->n;
    g->pc = g->cap;
    g->items = v;
    g->cap = c;
    g->n = g->an = 0;
    size_t k = 0;
    for (size_t i = 0; i < g->sn; ++i)
        if (g->frame - g->states[i].seen < g->state_ttl)
            g->states[k++] = g->states[i];
    g->sn = k;
    g->surface = NULL;
    g->in_frame = 0;
}
static inline void mulib_immgtk_native_free(mulib_immgtk_native_t *g) {
    if (!g) return;
    free(g->items);
    free(g->previous);
    free(g->states);
    free(g->actions);
    mulib_glx_drawlist_release(&g->draws);
    memset(g, 0, sizeof *g);
}
#endif
