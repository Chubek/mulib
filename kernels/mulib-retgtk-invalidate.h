/* MuLIB graphical extension: retgtk-invalidate. C11, header-only. */
#ifndef MULIB_RETGTK_INVALIDATE_H
#define MULIB_RETGTK_INVALIDATE_H
#include "mulib-retgtk-widget.h"
#include "mulib-glx-region.h"
/* Conservative single bounding rectangle: bounded storage and no overlapping
 * alpha repaint. Allocation failure leaves all pending widget state intact.
 * Collection does not consume damage; scene_repaint consumes it before paint. */
static inline int mulib_retgtk_invalidate_collect(mulib_retgtk_widget_t *w,
                                                mulib_glx_region_t *r)
{
    if (!w) return 1;
    while (w->parent) w = w->parent;
    if (w->damage_full || (w->dirty && !w->damage_pending))
        return mulib_glx_region_add(r, w->bounds);
    if (w->damage_pending) return mulib_glx_region_add(r, w->damage);
    return 1;
}
#endif
