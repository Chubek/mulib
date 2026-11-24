/* MuLIB notebook desktop extension. C11, header-only. SPDX-License-Identifier: MIT */
#ifndef MULIB_GLX_SCENE_H
#define MULIB_GLX_SCENE_H
#include "mulib-glx-scenenode.h"

/* Ordered siblings by insertion, depth-first parent-before-child painting.
 * Zero initialize. busy rejects API mutation/reentrant walking from callbacks.
 * Borrowed node pointers expire on mutation; callbacks must not write them. */
typedef struct {
    mulib_glx_scenenode *v;
    size_t n, cap;
    uint64_t next, revision;
    unsigned busy;
} mulib_glx_scene;
typedef struct {
    double x, y, sx, sy, opacity, l, t, r, b;
    int visible;
} mulib_glx_scene_world;

static inline mulib_glx_scenenode *mulib_glx_scene_find(mulib_glx_scene *s,
                                                       uint64_t id)
{
    if (s && id)
        for (size_t i = 0; i < s->n; i++)
            if (s->v[i].id == id)
                return s->v + i;
    return NULL;
}

static inline uint64_t mulib_glx_scene_add(mulib_glx_scene *s, uint64_t parent,
                                          const mulib_glx_scenenode *node)
{
    if (!s || s->busy || !mulib_glx_scenenode_valid(node) ||
        s->next == UINT64_MAX || s->revision == UINT64_MAX || s->n == SIZE_MAX ||
        (parent && !mulib_glx_scene_find(s, parent)))
        return 0;
    /* node may point into s->v, which grow can relocate. */
    mulib_glx_scenenode t = *node;
    if (!mgx_grow((void **)&s->v, &s->cap, s->n + 1, sizeof t) ||
        (t.buffer && !mulib_glx_clientbuffer_ref(t.buffer)))
        return 0;
    t.id = ++s->next;
    t.parent = parent;
    s->v[s->n++] = t;
    s->revision++;
    return t.id;
}

/* Replace view properties without changing identity, hierarchy or buffer
 * ownership. The supplied id, parent and buffer members are ignored. */
static inline int mulib_glx_scene_set_view(mulib_glx_scene *s, uint64_t id,
                                           const mulib_glx_scenenode *view)
{
    if (!s || s->busy || s->revision == UINT64_MAX || !view)
        return 0;
    mulib_glx_scenenode *node = mulib_glx_scene_find(s, id);
    if (!node)
        return 0;
    mulib_glx_scenenode next = *view;
    next.id = node->id;
    next.parent = node->parent;
    next.buffer = node->buffer;
    if (!mulib_glx_scenenode_valid(&next))
        return 0;
    *node = next;
    s->revision++;
    return 1;
}

static inline int mulib_glx_scene_reparent(mulib_glx_scene *s, uint64_t id,
                                           uint64_t parent)
{
    if (!s || s->busy || s->revision == UINT64_MAX)
        return 0;
    mulib_glx_scenenode *n = mulib_glx_scene_find(s, id);
    if (!n)
        return 0;
    uint64_t p = parent;
    for (size_t depth = 0; p; depth++) {
        mulib_glx_scenenode *q = mulib_glx_scene_find(s, p);
        if (!q || p == id || depth >= s->n)
            return 0;
        p = q->parent;
    }
    n->parent = parent;
    s->revision++;
    return 1;
}

static inline int mulib_glx_scene_is_descendant(mulib_glx_scene *s, uint64_t id,
                                               uint64_t ancestor)
{
    if (!s || !ancestor)
        return 0;
    for (size_t k = 0; id && k < s->n; k++) {
        if (id == ancestor)
            return 1;
        mulib_glx_scenenode *n = mulib_glx_scene_find(s, id);
        if (!n)
            return 0;
        id = n->parent;
    }
    return 0;
}

static inline int mulib_glx_scene_remove(mulib_glx_scene *s, uint64_t id)
{
    if (!s || s->busy || s->revision == UINT64_MAX ||
        !mulib_glx_scene_find(s, id))
        return 0;
    s->busy = 1;
    /* Leaves first, preserving ancestry until every child is detached. */
    for (;;) {
        size_t k = SIZE_MAX;
        for (size_t i = 0; i < s->n; i++) {
            if (!mulib_glx_scene_is_descendant(s, s->v[i].id, id))
                continue;
            int child = 0;
            for (size_t j = 0; j < s->n; j++)
                if (s->v[j].parent == s->v[i].id) {
                    child = 1;
                    break;
                }
            if (!child) {
                k = i;
                break;
            }
        }
        if (k == SIZE_MAX) {
            s->busy = 0;
            return 0; /* Corrupt public state; never loop forever. */
        }
        uint64_t removed = s->v[k].id;
        mulib_glx_clientbuffer *buffer = s->v[k].buffer;
        memmove(s->v + k, s->v + k + 1, (s->n - k - 1) * sizeof *s->v);
        s->n--;
        mulib_glx_clientbuffer_unref(buffer);
        if (removed == id)
            break;
    }
    s->revision++;
    s->busy = 0;
    return 1;
}

static inline mulib_glx_scene_world mulib_glx_scene_world_root(void)
{
    return (mulib_glx_scene_world){
        0, 0, 1, 1, 1, -INFINITY, -INFINITY, INFINITY, INFINITY, 1};
}

static inline int mulib_glx_scene_world_apply(mulib_glx_scene_world *w,
                                              const mulib_glx_scenenode *n)
{
    if (!w || !mulib_glx_scenenode_valid(n))
        return 0;
    w->x += n->x * w->sx;
    w->y += n->y * w->sy;
    w->sx *= n->sx;
    w->sy *= n->sy;
    w->opacity *= n->opacity;
    w->visible &= !!n->visible;
    if (!isfinite(w->x) || !isfinite(w->y) || !isfinite(w->sx) ||
        !isfinite(w->sy) || w->sx <= 0 || w->sy <= 0)
        return 0;
    if (n->clipped) {
        double l = w->x + n->clip.x * w->sx;
        double t = w->y + n->clip.y * w->sy;
        double r = w->x + ((double)n->clip.x + n->clip.w) * w->sx;
        double b = w->y + ((double)n->clip.y + n->clip.h) * w->sy;
        if (!isfinite(l) || !isfinite(t) || !isfinite(r) || !isfinite(b))
            return 0;
        w->l = fmax(w->l, l);
        w->t = fmax(w->t, t);
        w->r = fmin(w->r, r);
        w->b = fmin(w->b, b);
    }
    return 1;
}

static inline int mulib_glx_scene_world_at(mulib_glx_scene *s, uint64_t id,
                                           mulib_glx_scene_world *out)
{
    if (!s || !out || !id || !s->n ||
        s->n > SIZE_MAX / sizeof(mulib_glx_scenenode *))
        return 0;
    mulib_glx_scenenode **path = malloc(s->n * sizeof *path);
    if (!path)
        return 0;
    size_t count = 0;
    while (id) {
        mulib_glx_scenenode *p = mulib_glx_scene_find(s, id);
        if (!p || count == s->n) {
            free(path);
            return 0;
        }
        path[count++] = p;
        id = p->parent;
    }
    mulib_glx_scene_world w = mulib_glx_scene_world_root();
    while (count)
        if (!mulib_glx_scene_world_apply(&w, path[--count])) {
            free(path);
            return 0;
        }
    free(path);
    *out = w;
    return 1;
}

typedef int (*mulib_glx_scene_visit_fn)(
    void *, const mulib_glx_scenenode *, const mulib_glx_scene_world *);
typedef struct {
    mulib_glx_scenenode node;
    mulib_glx_scene_world world;
} mulib_glx_scene_command;
typedef struct {
    size_t next;
    uint64_t parent;
    mulib_glx_scene_world world;
} mulib_glx_scene_walk_level;

/* Validate/flatten the complete tree and pin every buffer BEFORE callbacks.
 * Callbacks receive temporary copies, must not mutate/free/reenter the scene,
 * and must not unref borrowed buffer references. Allocation/validation failures
 * invoke no callbacks. A callback returning 0 stops the walk. */
static inline int mulib_glx_scene_walk(mulib_glx_scene *s,
                                       mulib_glx_scene_visit_fn fn, void *u)
{
    if (!s || s->busy || !fn || s->n > s->cap || (s->n && !s->v) ||
        s->n >= SIZE_MAX / sizeof(mulib_glx_scene_walk_level) ||
        s->n > SIZE_MAX / sizeof(mulib_glx_scene_command))
        return 0;
    if (!s->n)
        return 1;
    mulib_glx_scene_command *cmd = malloc(s->n * sizeof *cmd);
    mulib_glx_scene_walk_level *stack = calloc(s->n + 1, sizeof *stack);
    if (!cmd || !stack) {
        free(cmd);
        free(stack);
        return 0;
    }
    s->busy = 1;
    size_t depth = 0, count = 0;
    int ok = 0;
    stack[0].world = mulib_glx_scene_world_root();
    for (;;) {
        mulib_glx_scene_walk_level *level = stack + depth;
        size_t i = level->next;
        while (i < s->n && s->v[i].parent != level->parent)
            i++;
        if (i == s->n) {
            if (!depth)
                break;
            depth--;
            continue;
        }
        level->next = i + 1;
        mulib_glx_scene_world w = level->world;
        if (count == s->n || !s->v[i].id ||
            !mulib_glx_scene_world_apply(&w, s->v + i))
            goto done;
        for (size_t j = 0; j < count; j++)
            if (cmd[j].node.id == s->v[i].id)
                goto done;
        if (s->v[i].buffer && !mulib_glx_clientbuffer_ref(s->v[i].buffer))
            goto done;
        cmd[count++] = (mulib_glx_scene_command){s->v[i], w};
        stack[++depth] = (mulib_glx_scene_walk_level){0, s->v[i].id, w};
    }
    if (count != s->n)
        goto done; /* Orphan, cycle or duplicate ID. */
    ok = 1;
    for (size_t i = 0; i < count; i++)
        if (!fn(u, &cmd[i].node, &cmd[i].world)) {
            ok = 0;
            break;
        }
done:
    for (size_t i = 0; i < count; i++)
        mulib_glx_clientbuffer_unref(cmd[i].node.buffer);
    free(cmd);
    free(stack);
    s->busy = 0;
    return ok;
}

/* Compatibility adapter: use the upstream straight-alpha operation exactly. */
static inline uint32_t mulib_glx_scene_over(uint32_t src, uint32_t dst,
                                           double opacity)
{
    return mgx_over(mgx_alpha(src, opacity), dst);
}

typedef struct {
    uint32_t *p;
    size_t stride;
    int w, h;
    mgx_rect clip;
} mulib_glx_scene_target;

static inline int mulib_glx_scene_paint_node(
    void *u, const mulib_glx_scenenode *n, const mulib_glx_scene_world *w)
{
    mulib_glx_scene_target *d = u;
    mulib_glx_clientbuffer *b = n->buffer;
    if (!w->visible || !b || !w->opacity)
        return 1;
    double l = fmax(d->clip.x, fmax(w->l, w->x));
    double t = fmax(d->clip.y, fmax(w->t, w->y));
    double r = fmin((double)d->clip.x + d->clip.w,
                    fmin(w->r, w->x + b->w * w->sx));
    double bot = fmin((double)d->clip.y + d->clip.h,
                      fmin(w->b, w->y + b->h * w->sy));
    if (l >= r || t >= bot)
        return 1;
    int x0 = (int)ceil(l - .5), y0 = (int)ceil(t - .5);
    int x1 = (int)ceil(r - .5), y1 = (int)ceil(bot - .5);
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            double sx = (x + .5 - w->x) / w->sx;
            double sy = (y + .5 - w->y) / w->sy;
            if (sx >= 0 && sy >= 0 && sx < b->w && sy < b->h) {
                uint32_t *p = d->p + (size_t)y * d->stride + (size_t)x;
                *p = mulib_glx_scene_over(
                    b->pixels[(size_t)sy * b->stride + (size_t)sx],
                    *p, w->opacity);
            }
        }
    return 1;
}

/* Explicit capacity adapter; destination clip is honored. Reject any overlap
 * with input storage before writing, even for currently hidden input nodes. */
static inline int mulib_glx_scene_render_surface(mulib_glx_scene *s,
                                                 mgx_surface *dst,
                                                 size_t words)
{
    size_t required;
    if (!s || s->busy || !mgx_surface_valid(dst) ||
        !mgx_surface_required_words(dst->stride, dst->w, dst->h, &required) ||
        required > words || s->n > s->cap || (s->n && !s->v))
        return 0;
    uintptr_t start = (uintptr_t)dst->pixels;
    uintptr_t end = start + required * sizeof(uint32_t);
    for (size_t i = 0; i < s->n; i++) {
        mulib_glx_clientbuffer *b = s->v[i].buffer;
        if (!b)
            continue;
        if (!mulib_glx_clientbuffer_valid(b))
            return 0;
        uintptr_t bs = (uintptr_t)b->pixels;
        uintptr_t be = bs + b->words * sizeof(uint32_t);
        if (start < be && bs < end)
            return 0;
    }
    mulib_glx_scene_target target = {
        dst->pixels, dst->stride, dst->w, dst->h, mgx_surface_clip(dst)};
    return mulib_glx_scene_walk(s, mulib_glx_scene_paint_node, &target);
}

static inline int mulib_glx_scene_render(mulib_glx_scene *s, uint32_t *p,
                                        size_t words, size_t stride, int w, int h)
{
    mgx_surface dst = {0};
    return mgx_wrap_capacity(&dst, p, words, stride, w, h) &&
           mulib_glx_scene_render_surface(s, &dst, words);
}

static inline void mulib_glx_scene_free(mulib_glx_scene *s)
{
    if (!s || s->busy)
        return;
    s->busy = 1;
    mulib_glx_scenenode *v = s->v;
    size_t count = s->n;
    s->v = NULL;
    s->n = s->cap = 0;
    for (size_t i = 0; i < count; i++)
        mulib_glx_clientbuffer_unref(v[i].buffer);
    free(v);
    memset(s, 0, sizeof *s);
}
#endif
