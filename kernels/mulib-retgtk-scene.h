/* MuLIB graphical extension: retgtk-scene. C11, header-only. */
#ifndef MULIB_RETGTK_SCENE_H
#define MULIB_RETGTK_SCENE_H
#include "mulib-retgtk-widget.h"
typedef struct {
    mulib_retgtk_widget_t *root;
    uint64_t generation;
    uint64_t painted_generation;
    mgx_surface *target;
    const uint32_t *pixels;
    const mulib_glx_font_t *font;
    mgui_theme theme;
    mgx_rect bounds;
    int width, height, initialized, drawing, force_full;
} mulib_retgtk_scene_t;
static inline void mulib_retgtk_scene_set(mulib_retgtk_scene_t *s,
                                         mulib_retgtk_widget_t *w)
{
    if (!s || (w && !mulib_retgtk_object_ref(&w->object))) return;
    mulib_retgtk_widget_t *old = s->root;
    s->root = w;
    s->generation++;
    s->force_full = 1;
    mulib_retgtk_widget_unref(old);
}
/* Hosts must call this after target loss, in-place font/theme asset mutation or
 * an expose when the persistent target's pixels are no longer preserved. */
static inline void mulib_retgtk_scene_invalidate(mulib_retgtk_scene_t *s)
{
    if (s) s->force_full = 1;
}
static inline int mret_scene_rect_equal(mgx_rect a, mgx_rect b)
{
    return a.x==b.x && a.y==b.y && a.w==b.w && a.h==b.h;
}
/* Incremental painting requires an unparented root and persistent, unmodified
 * CPU target with its full surface clip. Return -1 invalid/reentrant, 0 idle,
 * 1 repainted. Damage is
 * one clipped bounding rectangle; repaint includes background and every
 * intersecting layer, not just dirty widgets (correct for alpha/overlap).
 * No user callback may destroy/move the scene object itself during this call. */
static inline int mulib_retgtk_scene_repaint(mulib_retgtk_scene_t *s,
                                           mret_paint *p, mgx_rect bounds)
{
    if (!s || !p || !mgx_surface_valid(p->surface) || s->drawing) return -1;
    mgx_surface *target = p->surface;
    mgx_rect full = {0,0,target->w,target->h};
    if (!mret_scene_rect_equal(target->clip,full)) return -1;
    mulib_retgtk_widget_t *root = s->root;
    if (root && root->parent) return -1;
    if (root && !mulib_retgtk_object_ref(&root->object)) return -1;
    s->drawing = 1;
    uint64_t generation = s->generation;
    int changed = !s->initialized || s->force_full ||
        s->painted_generation!=generation || s->target!=target ||
        s->pixels!=target->pixels || s->width!=target->w || s->height!=target->h ||
        s->font!=p->font || memcmp(&s->theme,&p->theme,sizeof s->theme) ||
        !mret_scene_rect_equal(s->bounds,bounds);
    s->force_full = 0;
    if (root && (changed || root->dirty || root->layout_retry)) {
        root->layout_retry = 0;
        root->arranging = 1;
        mulib_retgtk_widget_arrange(root,bounds);
        root->arranging = 0;
    }
    mgx_rect damage = {0,0,0,0};
    if (changed || (root && (root->damage_full ||
                            (root->dirty && !root->damage_pending))))
        damage = full;
    else if (root && root->damage_pending)
        damage = mgx_intersect(full,root->damage);
    /* Clear before callbacks: invalidation during paint belongs to NEXT frame. */
    if (root) {
        root->damage_pending = root->damage_full = 0;
        root->damage = (mgx_rect){0,0,0,0};
        root->dirty = 0;
    }
    int painted = damage.w>0 && damage.h>0;
    if (painted) {
        target->clip = damage;
        /* SOURCE clear, not source-over: transparent backgrounds must not
         * accumulate the previous retained frame. */
        for (int y=damage.y;y<damage.y+damage.h;y++)
            for (int x=damage.x;x<damage.x+damage.w;x++)
                target->pixels[(size_t)y*target->stride+(size_t)x]=p->theme.background;
        mulib_retgtk_widget_paint(root,p);
        target->clip = full;
    }
    if (root && root->layout_retry) {
        root->dirty = root->damage_pending = root->damage_full = 1;
    }
    s->target = target; s->pixels = target->pixels;
    s->width = target->w; s->height = target->h;
    s->bounds = bounds; s->font = p->font; s->theme = p->theme;
    s->painted_generation = generation; s->initialized = 1;
    s->drawing = 0;
    mulib_retgtk_widget_unref(root);
    return painted;
}
/* Compatibility path: always paints and leaves clearing/background to caller.
 * Holding the root protects arrange/paint if a callback replaces the scene. */
static inline void mulib_retgtk_scene_draw(mulib_retgtk_scene_t *s,
                                          mret_paint *p, mgx_rect bounds)
{
    if (!s || s->drawing || !s->root ||
        !mulib_retgtk_object_ref(&s->root->object)) return;
    mulib_retgtk_widget_t *root = s->root;
    s->drawing = 1;
    mulib_retgtk_widget_arrange(root,bounds);
    root->damage_pending = root->damage_full = 0;
    root->damage = (mgx_rect){0,0,0,0};
    mulib_retgtk_widget_paint(root,p);
    s->drawing = 0;
    s->initialized = 0; /* Mixing with repaint requires a new full baseline. */
    mulib_retgtk_widget_unref(root);
}
#endif
