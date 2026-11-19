/* MuLIB graphical extension: retgtk-constraint. C11, header-only. */
#ifndef MULIB_RETGTK_CONSTRAINT_H
#define MULIB_RETGTK_CONSTRAINT_H
#include "mulib-retgtk-widget.h"
#include "mulib-retgtk-kind.h"
#include <limits.h>
typedef struct{double x,y,w,h;int min_w,min_h,max_w,max_h;}mret_constraint;
static inline mgx_rect mulib_retgtk_constraint_resolve(mret_constraint c,mgx_rect parent){int w=mgx_max(c.min_w,mgx_min(c.max_w,mgx_int(c.w*parent.w))),h=mgx_max(c.min_h,mgx_min(c.max_h,mgx_int(c.h*parent.h)));return (mgx_rect){mgx_int(parent.x+c.x*parent.w),mgx_int(parent.y+c.y*parent.h),w,h};}

/* A size-limiting container, independent of the older fractional resolver.
 * Its children overlay each other. Limits apply to the container's allocation
 * in a linear layout as well as to its children's measure/arrange passes. */
static inline mulib_gui_constraints_t mret_size_limits(mulib_gui_constraints_t c)
{
    c.min_w = mgx_max(0, c.min_w);
    c.min_h = mgx_max(0, c.min_h);
    c.max_w = mgx_max(c.min_w, c.max_w);
    c.max_h = mgx_max(c.min_h, c.max_h);
    return c;
}

static inline mulib_gui_constraints_t
mulib_retgtk_layout_constraints(const mulib_retgtk_widget_t *w)
{
    if (w && w->kind == MRET_KIND_CONSTRAINT && w->data)
        return *(const mulib_gui_constraints_t *)w->data;
    return (mulib_gui_constraints_t){0, 0, INT_MAX, INT_MAX};
}

static inline void mret_size_measure(mulib_retgtk_widget_t *w,
                                    mulib_gui_constraints_t c,
                                    mulib_gui_size_t *out)
{
    mulib_gui_constraints_t limits = mulib_retgtk_layout_constraints(w);
    c = mret_size_limits(c);
    c.min_w = c.min_h = 0;
    c.max_w = mgx_min(c.max_w, limits.max_w);
    c.max_h = mgx_min(c.max_h, limits.max_h);
    *out = (mulib_gui_size_t){0, 0};
    size_t n = 0;
    mulib_retgtk_widget_t **children =
        mulib_retgtk_widget_children_snapshot(w, &n);
    for (size_t i = 0; i < n; ++i) {
        if (children[i]->parent != w || !children[i]->visible) continue;
        mulib_gui_size_t s = mulib_retgtk_widget_measure(children[i], c);
        if (children[i]->parent != w || !children[i]->visible) continue;
        out->w = mgx_max(out->w, s.w);
        out->h = mgx_max(out->h, s.h);
    }
    mulib_retgtk_widget_children_release(children, n);
    out->w = mgx_max(limits.min_w, mgx_min(limits.max_w, out->w));
    out->h = mgx_max(limits.min_h, mgx_min(limits.max_h, out->h));
}

static inline void mret_size_arrange(mulib_retgtk_widget_t *w, mgx_rect r)
{
    mulib_gui_constraints_t c = mulib_retgtk_layout_constraints(w);
    /* In an undersized parent, preserve minima and let ancestor clipping apply. */
    r.w = mgx_max(c.min_w, mgx_min(c.max_w, mgx_max(0, r.w)));
    r.h = mgx_max(c.min_h, mgx_min(c.max_h, mgx_max(0, r.h)));
    size_t n = 0;
    mulib_retgtk_widget_t **children =
        mulib_retgtk_widget_children_snapshot(w, &n);
    for (size_t i = 0; i < n; ++i)
        if (children[i]->parent == w && children[i]->visible)
            mulib_retgtk_widget_arrange(children[i], r);
    mulib_retgtk_widget_children_release(children, n);
}

static inline void mret_size_destroy(mulib_retgtk_widget_t *w) { free(w->data); }
static const mulib_retgtk_widget_ops mret_size_ops = {
    mret_size_destroy, mret_size_measure, mret_size_arrange, NULL, NULL
};

/* Initialize with explicit maxima (INT_MAX means unbounded), not all zeroes. */
static inline mulib_retgtk_widget_t *
mulib_retgtk_constraint_box(uint64_t id, mulib_gui_constraints_t limits)
{
    if (limits.min_w < 0 || limits.min_h < 0 ||
        limits.max_w < limits.min_w || limits.max_h < limits.min_h) return NULL;
    mulib_retgtk_widget_t *w = mulib_retgtk_widget_create(id, &mret_size_ops);
    if (!w) return NULL;
    w->data = malloc(sizeof limits);
    if (!w->data) { mulib_retgtk_widget_unref(w); return NULL; }
    *(mulib_gui_constraints_t *)w->data = limits;
    w->kind = MRET_KIND_CONSTRAINT;
    return w;
}
#endif
