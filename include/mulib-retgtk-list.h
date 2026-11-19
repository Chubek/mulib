/* MuLIB graphical extension: retgtk-list. C11, header-only. */
#ifndef MULIB_RETGTK_LIST_H
#define MULIB_RETGTK_LIST_H
#include "mulib-retgtk-delegate.h"
#include "mulib-retgtk-kind.h"
typedef struct{const mulib_retgtk_model_t *model;size_t first,selected;int row_h;mulib_retgtk_delegate_t delegate;}mret_list_view;
static inline void mret_list_destroy(mulib_retgtk_widget_t *w){free(w->data);}
/* Synchronize borrowed model counts before painting or dispatching. This clears
 * out-of-range selection; stable row identities across reordering are not yet
 * part of the model contract. Invalid row heights disable the view safely. */
static inline size_t mret_list_sync(mulib_retgtk_widget_t *w)
{
    mret_list_view *v = w->data;
    if (!v || v->row_h <= 0 || !v->model || !v->model->rows ||
        !v->model->columns || !v->model->columns(v->model->user)) return 0;
    size_t n = v->model->rows(v->model->user);
    size_t page = w->bounds.h > 0 ? (size_t)(w->bounds.h / v->row_h) : 0;
    if (!page) page = 1;
    size_t last = n > page ? n - page : 0;
    if (v->first > last) v->first = last;
    if (v->selected >= n) v->selected = SIZE_MAX;
    return n;
}
static inline void mret_list_paint(mulib_retgtk_widget_t *w,mret_paint *p)
{
    mret_list_view *v = w->data;
    size_t n = mret_list_sync(w);
    if (!n || w->bounds.h <= 0 || w->bounds.w <= 0) return;
    size_t page = (size_t)(w->bounds.h / v->row_h) +
                  (w->bounds.h % v->row_h != 0);
    for (size_t row = 0; row < page && row < n - v->first; ++row) {
        mgx_rect r = {w->bounds.x,
            mgx_int((double)w->bounds.y + (double)row * v->row_h),
            w->bounds.w, v->row_h};
        mulib_retgtk_delegate_paint(&v->delegate, p, r,
            mulib_retgtk_model_cell(v->model, v->first + row, 0),
            v->first + row == v->selected);
    }
}
static inline int mret_list_event(mulib_retgtk_widget_t *w,const mgx_event *e)
{
    if (!e || !mulib_retgtk_widget_is_interactive(w)) return 0;
    mret_list_view *v = w->data;
    size_t n = mret_list_sync(w);
    if (!n || w->bounds.h <= 0 || w->bounds.w <= 0) return 0;
    if (e->type == MGX_EVENT_SCROLL && isfinite(e->dy)) {
        long double amount = fabsl((long double)e->dy) * 3;
        size_t delta = amount >= (long double)SIZE_MAX ? SIZE_MAX : (size_t)amount;
        if (e->dy > 0) v->first = delta >= v->first ? 0 : v->first - delta;
        else v->first += delta > n - v->first ? n - v->first : delta;
        (void)mret_list_sync(w);
        mulib_retgtk_widget_invalidate(w);
        return 1;
    }
    if (e->type == MGX_EVENT_BUTTON && e->code == 0 && !e->down &&
        mgx_contains(w->bounds, e->x, e->y)) {
        size_t row = (size_t)((e->y - w->bounds.y) / v->row_h);
        if (row < n - v->first) {
            v->selected = v->first + row;
            size_t selected = v->selected;
            mulib_retgtk_widget_invalidate(w);
            mulib_retgtk_signal_emit(&w->signals, w, MRET_CHANGED, &selected);
            return 1;
        }
    }
    return 0;
}
static const mulib_retgtk_widget_ops mret_list_ops={mret_list_destroy,NULL,NULL,mret_list_paint,mret_list_event};
static inline mulib_retgtk_widget_t *mulib_retgtk_list(uint64_t id,const mulib_retgtk_model_t *model){mulib_retgtk_widget_t *w=mulib_retgtk_widget_create(id,&mret_list_ops);if(!w)return NULL;mret_list_view *v=calloc(1,sizeof *v);if(!v){mulib_retgtk_widget_unref(w);return NULL;}v->model=model;v->selected=SIZE_MAX;v->row_h=24;w->data=v;w->focusable=1;w->kind=MRET_KIND_LIST;return w;}
#endif
