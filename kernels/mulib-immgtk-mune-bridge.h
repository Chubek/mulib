#ifndef MULIB_IMMGTK_MUNE_BRIDGE_H
#define MULIB_IMMGTK_MUNE_BRIDGE_H
#include "mulib-immgtk-native.h"
/* The bridge and GUI must outlive every VM callback. Never rebind a legacy host. */
#ifdef MULIB_GFX_ENABLE_MUNE
#include "mulib-mune.h"
typedef struct {
    mulib_immgtk_native_t *gui;
    mgx_rect clips[64];
    size_t n;
    uint64_t clip_frame;
} mulib_immgtk_mune_bridge_t;
static inline int mig_mune_num(mulib_mune_value_t v, double *d) {
    if (v.type == MULIB_MUNE_INTEGER) *d = (double)v.as.integer;
    else if (v.type == MULIB_MUNE_NUMBER) *d = v.as.number;
    else return 0;
    return isfinite(*d);
}
static inline int mig_mune_args(const mulib_mune_value_t *a, size_t n, double *d) {
    for (size_t i = 0; i < n; ++i) if (!mig_mune_num(a[i], d + i)) return 0;
    return 1;
}
static inline int mig_mune_bool(mulib_mune_result_t *r, int b) {
    mulib_mune_value_t v = {0};
    v.type = MULIB_MUNE_BOOLEAN;
    v.as.boolean = !!b;
    return mulib_mune_result_push(r, v);
}
static inline int mig_mune_number(mulib_mune_result_t *r, double d) {
    mulib_mune_value_t v = {0};
    v.type = MULIB_MUNE_NUMBER;
    v.as.number = d;
    return mulib_mune_result_push(r, v);
}
static inline int mig_mune_geometry(const double *v) {
    return fabs(v[0]) <= 1000000 && fabs(v[1]) <= 1000000 &&
        v[2] >= 0 && v[2] <= 1000000 && v[3] >= 0 && v[3] <= 1000000;
}
static inline mgx_rect mig_mune_rect_of(const double *d) {
    return (mgx_rect){mgx_int(floor(d[0])), mgx_int(floor(d[1])),
        mgx_int(ceil(d[2])), mgx_int(ceil(d[3]))};
}
static inline mulib_mune_status_t mig_mune_rect(mulib_mune_state_t *s,
        const mulib_mune_value_t *a, size_t n, mulib_mune_result_t *r, void *u) {
    (void)s; (void)r;
    mulib_immgtk_native_t *g = ((mulib_immgtk_mune_bridge_t *)u)->gui;
    double v[5];
    if (!g->in_frame || !g->surface || n != 5 || !mig_mune_args(a, n, v) ||
        !mig_mune_geometry(v) || v[4] < 0 || v[4] > UINT32_MAX ||
        floor(v[4]) != v[4]) return MULIB_MUNE_RUNTIME;
    mig_rect(g, mig_mune_rect_of(v), (uint32_t)v[4]);
    return g->error ? MULIB_MUNE_MEMORY : MULIB_MUNE_OK;
}
static inline mulib_mune_status_t mig_mune_text(mulib_mune_state_t *s,
        const mulib_mune_value_t *a, size_t n, mulib_mune_result_t *r, void *u) {
    (void)s; (void)r;
    mulib_immgtk_native_t *g = ((mulib_immgtk_mune_bridge_t *)u)->gui;
    double v[3];
    if (!g->in_frame || !g->surface || !g->font || n != 4 ||
        !mig_mune_args(a + 1, 3, v) || fabs(v[0]) > 1e6 || fabs(v[1]) > 1e6 ||
        v[2] < 0 || v[2] > UINT32_MAX || floor(v[2]) != v[2]) return MULIB_MUNE_RUNTIME;
    size_t len;
    const char *p = mulib_mune_value_string(a[0], &len);
    if (!p || !mgx_utf8_valid(p, len)) return MULIB_MUNE_RUNTIME;
    mig_flush(g);
    mgx_rect old = g->surface->clip;
    g->surface->clip = mgx_intersect(old, g->clip);
    mulib_glx_text_draw(g->surface, g->font, mgx_int(v[0]), mgx_int(v[1]), p, len,
        g->theme.font_size, (uint32_t)v[2]);
    g->surface->clip = old;
    return MULIB_MUNE_OK;
}
static inline mulib_mune_status_t mig_mune_pointer(mulib_mune_state_t *s,
        const mulib_mune_value_t *a, size_t n, mulib_mune_result_t *r, void *u) {
    (void)s; (void)a;
    mulib_immgtk_native_t *g = ((mulib_immgtk_mune_bridge_t *)u)->gui;
    if (n || !g->in_frame || !g->surface) return MULIB_MUNE_RUNTIME;
    return mig_mune_number(r, g->x) && mig_mune_number(r, g->y) ?
        MULIB_MUNE_OK : MULIB_MUNE_MEMORY;
}
static inline mulib_mune_status_t mig_mune_interact(mulib_mune_state_t *s,
        const mulib_mune_value_t *a, size_t n, mulib_mune_result_t *r, void *u) {
    (void)s;
    mulib_immgtk_native_t *g = ((mulib_immgtk_mune_bridge_t *)u)->gui;
    double v[5];
    if (!g->in_frame || !g->surface || n != 5 || !mig_mune_args(a, n, v) ||
        v[0] < 1 || v[0] > 9007199254740991. || floor(v[0]) != v[0] ||
        !mig_mune_geometry(v + 1) || v[3] <= 0 || v[4] <= 0) return MULIB_MUNE_RUNTIME;
    uint64_t id = (uint64_t)v[0];
    int c = mig_interact(g, id, mig_mune_rect_of(v + 1), 1);
    if (g->error) return MULIB_MUNE_RUNTIME;
    return mig_mune_bool(r, g->hot == id) && mig_mune_bool(r, g->active == id && g->down) &&
        mig_mune_bool(r, c) && mig_mune_bool(r, g->focus == id) ?
        MULIB_MUNE_OK : MULIB_MUNE_MEMORY;
}
static inline mulib_mune_status_t mig_mune_line(mulib_mune_state_t *s,
        const mulib_mune_value_t *a, size_t n, mulib_mune_result_t *r, void *u) {
    (void)s; (void)r;
    mulib_immgtk_native_t *g = ((mulib_immgtk_mune_bridge_t *)u)->gui;
    double v[6];
    if (!g->in_frame || !g->surface || n != 6 || !mig_mune_args(a, n, v) ||
        fabs(v[0]) > 1e6 || fabs(v[1]) > 1e6 || fabs(v[2]) > 1e6 ||
        fabs(v[3]) > 1e6 || v[4] <= 0 || v[4] > 1e6 ||
        v[5] < 0 || v[5] > UINT32_MAX || floor(v[5]) != v[5]) return MULIB_MUNE_RUNTIME;
    mgx_drawcmd c = {0};
    c.kind = MGX_LINE;
    c.clip = g->clip;
    c.a = (mgx_point){v[0], v[1]};
    c.b = (mgx_point){v[2], v[3]};
    c.width = v[4];
    c.color = (uint32_t)v[5];
    return mig_draw_add(g, c) ? MULIB_MUNE_OK : MULIB_MUNE_MEMORY;
}
static inline mulib_mune_status_t mig_mune_clip_push(mulib_mune_state_t *s,
        const mulib_mune_value_t *a, size_t n, mulib_mune_result_t *r, void *u) {
    (void)s; (void)r;
    mulib_immgtk_mune_bridge_t *b = u;
    double v[4];
    if (b->n && b->clip_frame != b->gui->frame) b->n = 0;
    if (!b->gui->in_frame || !b->gui->surface || b->n == 64 || n != 4 ||
        !mig_mune_args(a, n, v) || !mig_mune_geometry(v)) return MULIB_MUNE_RUNTIME;
    if (!b->n) b->clip_frame = b->gui->frame;
    b->clips[b->n++] = b->gui->clip;
    b->gui->clip = mgx_intersect(b->gui->clip, mig_mune_rect_of(v));
    return MULIB_MUNE_OK;
}
static inline mulib_mune_status_t mig_mune_clip_pop(mulib_mune_state_t *s,
        const mulib_mune_value_t *a, size_t n, mulib_mune_result_t *r, void *u) {
    (void)s; (void)a; (void)r;
    mulib_immgtk_mune_bridge_t *b = u;
    if (!b->gui->in_frame || !b->gui->surface || n || !b->n ||
        b->clip_frame != b->gui->frame) return MULIB_MUNE_RUNTIME;
    b->gui->clip = b->clips[--b->n];
    return MULIB_MUNE_OK;
}
/* Failed registration may leave a prefix installed; discard that VM on failure. */
static inline int mulib_immgtk_mune_bridge_bind(mulib_mune_state_t *s,
        mulib_immgtk_mune_bridge_t *b, mulib_immgtk_native_t *g) {
    if (!s || !b || !g) return 0;
    memset(b, 0, sizeof *b);
    b->gui = g;
    return mulib_mune_bind(s, "imm_rect", mig_mune_rect, b) == MULIB_MUNE_OK &&
        mulib_mune_bind(s, "imm_text", mig_mune_text, b) == MULIB_MUNE_OK &&
        mulib_mune_bind(s, "imm_pointer", mig_mune_pointer, b) == MULIB_MUNE_OK &&
        mulib_mune_bind(s, "imm_interact", mig_mune_interact, b) == MULIB_MUNE_OK &&
        mulib_mune_bind(s, "imm_line", mig_mune_line, b) == MULIB_MUNE_OK &&
        mulib_mune_bind(s, "imm_clip_push", mig_mune_clip_push, b) == MULIB_MUNE_OK &&
        mulib_mune_bind(s, "imm_clip_pop", mig_mune_clip_pop, b) == MULIB_MUNE_OK;
}
static inline void mulib_immgtk_mune_bridge_reset(mulib_immgtk_mune_bridge_t *b) {
    if (!b || !b->gui) return;
    if (b->n && b->gui->in_frame && b->clip_frame == b->gui->frame)
        b->gui->clip = b->clips[0];
    b->n = 0;
}
/* Recommended script entry: restore clip scopes on success AND runtime error.
 * Mune result ownership and source side-effect semantics are unchanged. */
static inline mulib_mune_status_t mulib_immgtk_mune_bridge_run(mulib_mune_state_t *s,
        mulib_immgtk_mune_bridge_t *b, const char *source, size_t length,
        mulib_mune_result_t **result, mulib_mune_error_t *error) {
    if (!s || !b || !b->gui || !b->gui->in_frame || !b->gui->surface)
        return MULIB_MUNE_INVALID;
    mulib_immgtk_mune_bridge_reset(b);
    mulib_mune_status_t status = mulib_mune_run_source(s, source, length, result, error);
    mulib_immgtk_mune_bridge_reset(b);
    return status;
}
#endif
#endif
