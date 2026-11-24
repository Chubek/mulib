/* MuLIB graphical extension: retgtk-scroll. C11, header-only. */
#ifndef MULIB_RETGTK_SCROLL_H
#define MULIB_RETGTK_SCROLL_H
#include "mulib-retgtk-widget.h"
#include "mulib-retgtk-kind.h"
static inline double mret_scroll_limit(const mulib_retgtk_widget_t *w)
{
    return isfinite(w->hi) ? fmax(0,w->hi-mgx_max(0,w->bounds.h)) : 0;
}
static inline void mret_scroll_arrange(mulib_retgtk_widget_t *w, mgx_rect r)
{
    double content = isfinite(w->hi) ? fmax(0,w->hi) : 0;
    w->value = mgx_clamp(isfinite(w->value)?w->value:0,0,mret_scroll_limit(w));
    mgx_rect child = {r.x,mgx_int((double)r.y-w->value),
                     mgx_max(0,r.w),mgx_max(mgx_max(0,r.h),mgx_int(content))};
    size_t n=0;
    mulib_retgtk_widget_t **children = mulib_retgtk_widget_children_snapshot(w,&n);
    if (w->n && !children) mulib_retgtk_widget_layout_retry(w);
    for (size_t i=0;i<n;i++)
        if (children[i]->parent==w && children[i]->visible)
            mulib_retgtk_widget_arrange(children[i],child);
    mulib_retgtk_widget_children_release(children,n);
}
/* Programmatic scroll is immediate, so hit testing sees committed geometry
 * even before the next draw. Returns 0 when clamped/no change. */
static inline int mulib_retgtk_scroll_to(mulib_retgtk_widget_t *w, double offset)
{
    if (!w || w->kind!=MRET_KIND_SCROLL || !isfinite(offset) ||
        !mulib_retgtk_object_ref(&w->object)) return 0;
    double next=mgx_clamp(offset,0,mret_scroll_limit(w));
    if (w->value==next) { mulib_retgtk_widget_unref(w); return 0; }
    w->value=next;
    mulib_retgtk_widget_invalidate(w);
    mret_scroll_arrange(w,w->bounds);
    mulib_retgtk_widget_unref(w);
    return 1;
}
static inline int mulib_retgtk_scroll_content(mulib_retgtk_widget_t *w,
                                            int content_height)
{
    if (!w || w->kind!=MRET_KIND_SCROLL || content_height<0 ||
        !mulib_retgtk_object_ref(&w->object)) return 0;
    w->hi=content_height;
    mulib_retgtk_widget_invalidate(w);
    mret_scroll_arrange(w,w->bounds);
    mulib_retgtk_widget_unref(w);
    return 1;
}
static inline int mret_scroll_event(mulib_retgtk_widget_t *w,const mgx_event *e)
{
    if (!e || e->type!=MGX_EVENT_SCROLL || !isfinite(e->dy) ||
        !mulib_retgtk_widget_is_interactive(w)) return 0;
    long double next=(long double)w->value-(long double)e->dy*30;
    double limit=mret_scroll_limit(w);
    return mulib_retgtk_scroll_to(w,next<=0?0:next>=limit?limit:(double)next);
}
static const mulib_retgtk_widget_ops mret_scroll_ops = {
    NULL,NULL,mret_scroll_arrange,NULL,mret_scroll_event
};
static inline mulib_retgtk_widget_t *mulib_retgtk_scroll(uint64_t id,int content)
{
    mulib_retgtk_widget_t *w=mulib_retgtk_widget_create(id,&mret_scroll_ops);
    if (w) { w->hi=mgx_max(0,content); w->kind=MRET_KIND_SCROLL; }
    return w;
}
#endif
