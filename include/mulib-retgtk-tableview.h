/* MuLIB graphical extension: retgtk-tableview. C11, header-only. */
#ifndef MULIB_RETGTK_TABLEVIEW_H
#define MULIB_RETGTK_TABLEVIEW_H
#include "mulib-retgtk-list.h"
static inline void mret_tableview_paint(mulib_retgtk_widget_t *w,mret_paint *p)
{
    mret_list_view *v = w->data;
    size_t n = mret_list_sync(w);
    if (!n || w->bounds.h <= 0 || w->bounds.w <= 0) return;
    size_t cols = v->model->columns(v->model->user);
    size_t page = (size_t)(w->bounds.h / v->row_h) +
                  (w->bounds.h % v->row_h != 0);
    /* Bound work by visible pixels, even if the source has many columns.
     * Uniform subpixel columns are omitted instead of drawing empty cells. */
    size_t visible = cols < (size_t)w->bounds.w ? cols : (size_t)w->bounds.w;
    for (size_t row = 0; row < page && row < n - v->first; ++row)
        for (size_t slot = 0; slot < visible; ++slot) {
            size_t j = slot;
            if (visible != cols) {
                /* Last column of this pixel bucket: its uniform right edge
                 * advances by a pixel. Compute ceil((slot+1)*cols/visible)-1
                 * without overflowing size_t, including 32-bit targets. */
                uint64_t product = (uint64_t)(slot + 1) * (cols % visible);
                j = (slot + 1) * (cols / visible) +
                    (size_t)(product / visible) +
                    (product % visible != 0) - 1;
            }
            int a = (int)((long double)slot * w->bounds.w / visible);
            int b = (int)((long double)(slot + 1) * w->bounds.w / visible);
            mgx_rect r = {
                mgx_int((double)w->bounds.x + a),
                mgx_int((double)w->bounds.y + (double)row * v->row_h),
                b - a, v->row_h - 1};
            mulib_retgtk_delegate_paint(&v->delegate, p, r,
                mulib_retgtk_model_cell(v->model, v->first + row, j),
                v->first + row == v->selected);
        }
}
static const mulib_retgtk_widget_ops mret_tableview_ops={mret_list_destroy,NULL,NULL,mret_tableview_paint,mret_list_event};
static inline mulib_retgtk_widget_t *mulib_retgtk_tableview(uint64_t id,const mulib_retgtk_model_t *m){mulib_retgtk_widget_t *w=mulib_retgtk_list(id,m);if(w){w->ops=&mret_tableview_ops;w->kind=MRET_KIND_TABLEVIEW;}return w;}
#endif
