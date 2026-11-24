/* MuLIB graphical extension: retgtk-grid. C11, header-only. */
#ifndef MULIB_RETGTK_GRID_H
#define MULIB_RETGTK_GRID_H
#include "mulib-retgtk-layout.h"
static inline size_t mret_grid_count(const mulib_retgtk_widget_t *w)
{
    size_t n = 0;
    for (size_t i = 0; i < w->n; ++i) n += w->children[i]->visible != 0;
    return n;
}
static inline void mret_grid_measure(mulib_retgtk_widget_t *w,
    mulib_gui_constraints_t c, mulib_gui_size_t *s)
{
    (void)c;
    size_t n = 0, snapshot_n = 0, cols = w->kind ? w->kind : 1;
    mulib_retgtk_widget_t **children =
        mulib_retgtk_widget_children_snapshot(w, &snapshot_n);
    if (w->n && !children) mulib_retgtk_widget_layout_retry(w);
    int width = 0, height = 0, pad = mgx_max(0, mgx_int(w->lo));
    for (size_t i = 0; i < snapshot_n; ++i) {
        if (children[i]->parent != w || !children[i]->visible) continue;
        mulib_gui_size_t item = mulib_retgtk_widget_measure(children[i],
            (mulib_gui_constraints_t){0, 0, INT_MAX, INT_MAX});
        if (children[i]->parent != w || !children[i]->visible) continue;
        width = mgx_max(width, item.w);
        height = mgx_max(height, item.h);
        ++n;
    }
    mulib_retgtk_widget_children_release(children, snapshot_n);
    size_t rows = n ? 1 + (n - 1) / cols : 0;
    *s = (mulib_gui_size_t){
        n ? mret_layout_int(((long double)width + 2.L * pad) * cols) : 0,
        mret_layout_int(((long double)height + 2.L * pad) * rows)};
}
static inline void mret_grid_arrange(mulib_retgtk_widget_t *w, mgx_rect r)
{
    size_t n = mret_grid_count(w), cols = w->kind ? w->kind : 1;
    if (!n) return;
    size_t snapshot_n = 0;
    mulib_retgtk_widget_t **children =
        mulib_retgtk_widget_children_snapshot(w, &snapshot_n);
    if (w->n && !children) mulib_retgtk_widget_layout_retry(w);
    size_t rows = 1 + (n - 1) / cols, k = 0;
    r.w = mgx_max(0, r.w);
    r.h = mgx_max(0, r.h);
    for (size_t i = 0; i < snapshot_n; ++i) {
        if (children[i]->parent != w || !children[i]->visible) continue;
        size_t x = k % cols, y = k / cols;
        int a = (int)((long double)x * r.w / cols);
        int b = (int)((long double)(x + 1) * r.w / cols);
        int c = (int)((long double)y * r.h / rows);
        int d = (int)((long double)(y + 1) * r.h / rows);
        mgx_rect cell = {mret_layout_int((long double)r.x + a),
            mret_layout_int((long double)r.y + c), b - a, d - c};
        mulib_retgtk_widget_arrange(children[i],
            mret_layout_inset(cell, mgx_int(w->lo)));
        ++k;
    }
    mulib_retgtk_widget_children_release(children, snapshot_n);
}
static const mulib_retgtk_widget_ops mret_grid_ops={NULL,mret_grid_measure,mret_grid_arrange,NULL,NULL};
static inline mulib_retgtk_widget_t *mulib_retgtk_grid(uint64_t id,unsigned cols,int padding){if(!cols)return NULL;mulib_retgtk_widget_t *w=mulib_retgtk_widget_create(id,&mret_grid_ops);if(w){w->kind=cols;w->lo=mgx_max(0,padding);}return w;}
#endif
