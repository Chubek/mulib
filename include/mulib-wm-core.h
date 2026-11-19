/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WM_CORE_H
#define MULIB_WM_CORE_H
#include "mulib-glx-core.h"
typedef struct { int32_t x, y, w, h; } mulib_wm_rect;

static inline int mulib_wm_rect_valid(mulib_wm_rect r)
{
    return r.w >= 0 && r.h >= 0 && (int64_t)r.x + r.w <= INT32_MAX &&
           (int64_t)r.y + r.h <= INT32_MAX;
}
static inline int mulib_wm_contains(mulib_wm_rect r, double x, double y)
{
    return mulib_wm_rect_valid(r) && x >= r.x && y >= r.y &&
           x < (double)r.x + r.w && y < (double)r.y + r.h;
}
static inline mulib_wm_rect mulib_wm_intersect(mulib_wm_rect a, mulib_wm_rect b)
{
    if (!mulib_wm_rect_valid(a) || !mulib_wm_rect_valid(b))
        return (mulib_wm_rect){0};
    int32_t x = a.x > b.x ? a.x : b.x, y = a.y > b.y ? a.y : b.y;
    int32_t r = a.x + a.w < b.x + b.w ? a.x + a.w : b.x + b.w;
    int32_t t = a.y + a.h < b.y + b.h ? a.y + a.h : b.y + b.h;
    /* Overlap cannot exceed either input extent, even at INT32_MIN. */
    return (mulib_wm_rect){x, y, r > x ? r - x : 0, t > y ? t - y : 0};
}
/* Explicit checked conversions: neither public rectangle type is aliased. */
static inline int mulib_wm_rect_from_mgx(mgx_rect r, mulib_wm_rect *out)
{
    if (!out || r.w < 0 || r.h < 0)
        return 0;
#if INT_MIN < INT32_MIN
    if (r.x < INT32_MIN || r.y < INT32_MIN)
        return 0;
#endif
#if INT_MAX > INT32_MAX
    if (r.x > INT32_MAX || r.y > INT32_MAX || r.w > INT32_MAX || r.h > INT32_MAX)
        return 0;
#endif
    mulib_wm_rect v = {(int32_t)r.x, (int32_t)r.y, (int32_t)r.w, (int32_t)r.h};
    if (!mulib_wm_rect_valid(v))
        return 0;
    *out = v;
    return 1;
}
static inline int mulib_wm_rect_to_mgx(mulib_wm_rect r, mgx_rect *out)
{
    if (!out || !mulib_wm_rect_valid(r))
        return 0;
#if INT_MIN > INT32_MIN
    if (r.x < INT_MIN || r.y < INT_MIN)
        return 0;
#endif
#if INT_MAX < INT32_MAX
    if (r.x > INT_MAX || r.y > INT_MAX || r.w > INT_MAX || r.h > INT_MAX)
        return 0;
#endif
    *out = (mgx_rect){(int)r.x, (int)r.y, (int)r.w, (int)r.h};
    return 1;
}
/* Preserve the shared helper signature and overflow/ownership contract. */
static inline int mulib_wm_grow(void **p, size_t *cap, size_t n, size_t elem)
{
    return mgx_grow(p, cap, n, elem);
}
static inline char *mulib_wm_strdup(const char *s)
{
    if (!s)
        return NULL;
    size_t n = strlen(s);
    if (n == SIZE_MAX)
        return NULL;
    char *p = (char *)malloc(n + 1);
    if (p)
        memcpy(p, s, n + 1);
    return p;
}
typedef enum {
    MULIB_WM_X11 = 1, MULIB_WM_WAYLAND = 2, MULIB_WM_XWAYLAND = 3
} mulib_wm_backend;
enum {
    MULIB_WM_MAPPED = 1, MULIB_WM_MINIMIZED = 2, MULIB_WM_FULLSCREEN = 4,
    MULIB_WM_MAXIMIZED = 8, MULIB_WM_FLOATING = 16
};
/* Original layouts retained; lifecycle state lives in the opt-in sidecar below. */
typedef struct {
    uint64_t id, native, workspace, cell, parent;
    mulib_wm_backend backend;
    mulib_wm_rect rect;
    unsigned flags;
    char *title, *app_id;
    uint64_t configure_serial;
} mulib_wm_window;
typedef struct {
    mulib_wm_window *v;
    size_t n, cap;
    uint64_t next, focused, revision;
} mulib_wm_core;

static inline mulib_wm_window *mulib_wm_find(mulib_wm_core *c, uint64_t id)
{
    if (c && id)
        for (size_t i = 0; i < c->n; i++)
            if (c->v[i].id == id)
                return c->v + i;
    return NULL;
}
static inline uint64_t mulib_wm_add(mulib_wm_core *c, mulib_wm_backend b,
                                   uint64_t native, const char *title, const char *app)
{
    if (!c || !native || b < 1 || b > 3 || c->next == UINT64_MAX ||
        c->revision == UINT64_MAX || c->n == SIZE_MAX)
        return 0;
    for (size_t i = 0; i < c->n; i++)
        if (c->v[i].backend == b && c->v[i].native == native)
            return 0;
    char *t = mulib_wm_strdup(title ? title : ""), *a = mulib_wm_strdup(app ? app : "");
    if (!t || !a || !mulib_wm_grow((void **)&c->v, &c->cap, c->n + 1, sizeof *c->v)) {
        free(t);
        free(a);
        return 0;
    }
    uint64_t id = ++c->next;
    c->v[c->n++] = (mulib_wm_window){
        .id = id, .native = native, .backend = b, .title = t, .app_id = a
    };
    c->revision++;
    return id;
}
static inline int mulib_wm_remove(mulib_wm_core *c, uint64_t id)
{
    mulib_wm_window *w = mulib_wm_find(c, id);
    if (!w || c->revision == UINT64_MAX)
        return 0;
    size_t i = (size_t)(w - c->v);
    free(w->title);
    free(w->app_id);
    memmove(w, w + 1, (c->n - i - 1) * sizeof *w);
    c->n--;
    if (c->focused == id)
        c->focused = 0;
    for (i = 0; i < c->n; i++)
        if (c->v[i].parent == id)
            c->v[i].parent = 0;
    c->revision++;
    return 1;
}
static inline int mulib_wm_place(mulib_wm_core *c, uint64_t id, mulib_wm_rect r)
{
    mulib_wm_window *w = mulib_wm_find(c, id);
    if (!w || c->revision == UINT64_MAX || !mulib_wm_rect_valid(r) || !r.w || !r.h)
        return 0;
    w->rect = r;
    c->revision++;
    return 1;
}
static inline void mulib_wm_core_free(mulib_wm_core *c)
{
    if (c) {
        for (size_t i = 0; i < c->n; i++) {
            free(c->v[i].title);
            free(c->v[i].app_id);
        }
        free(c->v);
        memset(c, 0, sizeof *c);
    }
}

/* Transport acceptance is NOT a native lifecycle event. The backend receives
 * copied IDs/geometry and must not re-enter or mutate this core/sidecar. */
typedef struct {
    void *user;
    int (*map)(void *, uint64_t, int);
    int (*configure)(void *, uint64_t, mulib_wm_rect, uint64_t);
    int (*focus)(void *, uint64_t);
} mulib_wm_native_ops;
typedef struct {
    uint64_t id, serial;
    mulib_wm_rect pending;
    int acked;
} mulib_wm_native_state;
typedef struct {
    mulib_wm_core *core; /* Borrowed; must outlive sidecar. */
    mulib_wm_native_state *v;
    size_t n, cap, limit;
    uint64_t epoch, next_serial;
    mulib_wm_native_ops ops;
    int busy;
} mulib_wm_lifecycle;
static inline mulib_wm_native_state *mulib_wm_lifecycle_find(mulib_wm_lifecycle *s,
                                                           uint64_t id)
{
    if (s && id)
        for (size_t i = 0; i < s->n; i++)
            if (s->v[i].id == id)
                return s->v + i;
    return NULL;
}
static inline int mulib_wm_lifecycle_track(mulib_wm_lifecycle *s, uint64_t id)
{
    if (!s || s->busy || !mulib_wm_find(s->core, id) ||
        mulib_wm_lifecycle_find(s, id) || s->n >= s->limit || s->epoch == UINT64_MAX ||
        !mulib_wm_grow((void **)&s->v, &s->cap, s->n + 1, sizeof *s->v))
        return 0;
    s->v[s->n++] = (mulib_wm_native_state){.id = id};
    s->epoch++;
    return 1;
}
/* Mark host policy/layout changes not made by lifecycle methods. Never wrap. */
static inline int mulib_wm_lifecycle_invalidate(mulib_wm_lifecycle *s)
{
    if (!s || s->busy || s->epoch == UINT64_MAX)
        return 0;
    s->epoch++;
    return 1;
}
static inline int mulib_wm_request_map(mulib_wm_lifecycle *s, uint64_t id, int map)
{
    mulib_wm_window *w = s ? mulib_wm_find(s->core, id) : NULL;
    if (!w || s->busy || !mulib_wm_lifecycle_find(s, id) || !s->ops.map)
        return 0;
    uint64_t native = w->native;
    s->busy = 1;
    int ok = s->ops.map(s->ops.user, native, !!map);
    s->busy = 0;
    return ok;
}
/* Call only on confirmed map/unmap (or explicit headless host transition). */
static inline int mulib_wm_mapped(mulib_wm_lifecycle *s, uint64_t id, int map)
{
    mulib_wm_native_state *v = mulib_wm_lifecycle_find(s, id);
    mulib_wm_window *w = s ? mulib_wm_find(s->core, id) : NULL;
    if (!v || !w || s->busy || s->epoch == UINT64_MAX ||
        s->core->revision == UINT64_MAX)
        return 0;
    if (map) {
        w->flags |= MULIB_WM_MAPPED;
    } else {
        w->flags &= ~MULIB_WM_MAPPED;
        v->serial = 0;
        v->acked = 0;
        if (s->core->focused == id)
            s->core->focused = 0;
    }
    s->epoch++;
    s->core->revision++;
    return 1;
}
static inline uint64_t mulib_wm_request_configure(mulib_wm_lifecycle *s, uint64_t id,
                                                mulib_wm_rect r)
{
    mulib_wm_native_state *v = mulib_wm_lifecycle_find(s, id);
    mulib_wm_window *w = s ? mulib_wm_find(s->core, id) : NULL;
    if (!v || !w || s->busy || !s->ops.configure || s->next_serial == UINT64_MAX ||
        !mulib_wm_rect_valid(r) || !r.w || !r.h)
        return 0;
    uint64_t serial = ++s->next_serial, native = w->native;
    s->busy = 1;
    int ok = s->ops.configure(s->ops.user, native, r, serial);
    s->busy = 0;
    if (!ok)
        return 0;
    v->pending = r;
    v->serial = serial;
    v->acked = 0;
    return serial;
}
static inline int mulib_wm_configure_ack(mulib_wm_lifecycle *s, uint64_t id,
                                        uint64_t serial)
{
    mulib_wm_native_state *v = mulib_wm_lifecycle_find(s, id);
    if (!v || s->busy || !serial || v->serial != serial)
        return 0;
    v->acked = 1;
    return 1;
}
/* Wayland: host must correlate serial with the relevant actual buffer commit.
 * X11: call after matching ConfigureNotify, not after writing ConfigureWindow. */
static inline int mulib_wm_configure_commit(mulib_wm_lifecycle *s, uint64_t id,
                                           uint64_t serial)
{
    mulib_wm_native_state *v = mulib_wm_lifecycle_find(s, id);
    mulib_wm_window *w = s ? mulib_wm_find(s->core, id) : NULL;
    if (!v || !w || s->busy || !serial || v->serial != serial ||
        (w->backend == MULIB_WM_WAYLAND && !v->acked) ||
        s->epoch == UINT64_MAX || s->core->revision == UINT64_MAX)
        return 0;
    w->rect = v->pending;
    w->configure_serial = serial;
    v->serial = 0; /* The ack is not perpetual permission for future buffers. */
    v->acked = 0;
    s->epoch++;
    s->core->revision++;
    return 1;
}
static inline int mulib_wm_destroyed(mulib_wm_lifecycle *s, uint64_t id)
{
    mulib_wm_native_state *v = mulib_wm_lifecycle_find(s, id);
    if (!v || s->busy || s->epoch == UINT64_MAX || !mulib_wm_remove(s->core, id))
        return 0;
    size_t i = (size_t)(v - s->v);
    memmove(v, v + 1, (s->n - i - 1) * sizeof *v);
    s->n--;
    s->epoch++;
    return 1;
}
static inline void mulib_wm_lifecycle_free(mulib_wm_lifecycle *s)
{
    if (s && !s->busy) {
        free(s->v);
        memset(s, 0, sizeof *s);
    }
}
#endif
